/**
 * @file     GlyphAtlas.cpp
 * @brief    Implementation of `GlyphAtlas::GetOrCreate()`
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-21
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "GlyphAtlas.hpp"

#include <msdfgen-ext.h>
#include <msdfgen.h>

#include <cmath>
#include <vector>

namespace ImFrame::Internal {

namespace {

/// Corner-angle threshold (radians) below which a vertex is treated as a sharp corner needing its
/// own edge color — msdfgen's own commonly-used default (~172 degrees external angle).
constexpr double kAngleThreshold = 3.0;

} // namespace

std::size_t GlyphAtlas::CacheKeyHash::operator()(const CacheKey& key) const noexcept {
    const std::size_t h1 = std::hash<std::uint32_t>{}(key.Font.Value());
    const std::size_t h2 = std::hash<std::uint32_t>{}(key.GlyphId);
    return h1 ^ (h2 + 0x9e3779b9U + (h1 << 6) + (h1 >> 2));
}

GlyphAtlas::GlyphAtlas(std::uint32_t atlasWidth, std::uint32_t atlasHeight) : _atlas(atlasWidth, atlasHeight) {}

const GlyphAtlasEntry* GlyphAtlas::GetOrCreate(const FontRegistry& registry, Rendering::FontId font,
                                                std::uint32_t glyphId) {
    const CacheKey key{font, glyphId};
    const auto     found = _cache.find(key);
    if (found != _cache.end()) { return &found->second; }

    const FontFace* face = registry.Get(font);
    if (face == nullptr) { return nullptr; }

    // Wraps the FT_Face FontRegistry already owns -- no separate FreeType library instance or
    // duplicate font load. Adopted/destroyed around this one call; see this file's header comment.
    msdfgen::FontHandle* msdfFont = msdfgen::adoptFreetypeFont(face->Face());
    if (msdfFont == nullptr) { return nullptr; }

    msdfgen::Shape shape;
    const bool loaded = msdfgen::loadGlyph(shape, msdfFont, msdfgen::GlyphIndex(glyphId),
                                            msdfgen::FONT_SCALING_EM_NORMALIZED, nullptr);
    msdfgen::destroyFont(msdfFont);

    if (!loaded) { return nullptr; }

    if (shape.contours.empty()) {
        // A blank glyph (space, zero-width joiner, ...) is valid -- no bitmap to generate or upload.
        return &_cache.emplace(key, GlyphAtlasEntry{.Texture = _atlas.TextureId(), .IsBlank = true}).first->second;
    }

    shape.normalize();
    msdfgen::edgeColoringSimple(shape, kAngleThreshold);

    const double                border = kPxRange / kPixelsPerEm;
    const msdfgen::Shape::Bounds bounds = shape.getBounds(border);
    const double                 boundsWidth  = bounds.r - bounds.l;
    const double                 boundsHeight = bounds.t - bounds.b;

    if (boundsWidth <= 0.0 || boundsHeight <= 0.0) {
        // Contours present but a degenerate (zero-area) bounding box -- treat the same as blank
        // rather than attempting to allocate a zero-size atlas region.
        return &_cache.emplace(key, GlyphAtlasEntry{.Texture = _atlas.TextureId(), .IsBlank = true}).first->second;
    }

    const auto bitmapWidth  = static_cast<std::uint32_t>(std::ceil(boundsWidth * kPixelsPerEm));
    const auto bitmapHeight = static_cast<std::uint32_t>(std::ceil(boundsHeight * kPixelsPerEm));

    const auto region = _atlas.Alloc(bitmapWidth, bitmapHeight);
    if (!region) { return nullptr; } // atlas full and cannot grow further

    const msdfgen::Vector2           scale(kPixelsPerEm, kPixelsPerEm);
    const msdfgen::Vector2           translate(-bounds.l, -bounds.b);
    const msdfgen::SDFTransformation transformation(msdfgen::Projection(scale, translate), msdfgen::Range(border));

    // Y_DOWNWARD: msdfgen writes row 0 as the visual top, matching TextureAtlas's/every other RGBA8
    // buffer's row-major top-down convention in this codebase -- no manual post-generation row flip needed.
    msdfgen::Bitmap<float, 4> mtsdf(static_cast<int>(bitmapWidth), static_cast<int>(bitmapHeight),
                                    msdfgen::Y_DOWNWARD);
    msdfgen::generateMTSDF(mtsdf, shape, transformation);

    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(bitmapWidth) * bitmapHeight * 4);
    for (std::uint32_t y = 0; y < bitmapHeight; ++y) {
        for (std::uint32_t x = 0; x < bitmapWidth; ++x) {
            const float*      px     = mtsdf(static_cast<int>(x), static_cast<int>(y));
            const std::size_t offset = (static_cast<std::size_t>(y) * bitmapWidth + x) * 4;
            rgba[offset + 0]         = msdfgen::pixelFloatToByte(px[0]);
            rgba[offset + 1]         = msdfgen::pixelFloatToByte(px[1]);
            rgba[offset + 2]         = msdfgen::pixelFloatToByte(px[2]);
            rgba[offset + 3]         = msdfgen::pixelFloatToByte(px[3]);
        }
    }

    _atlas.Upload(*region, rgba);
    const auto [uvMin, uvMax] = _atlas.Uv(*region);

    GlyphAtlasEntry entry{
        .Texture   = _atlas.TextureId(),
        .Region    = *region,
        .UvMin     = uvMin,
        .UvMax     = uvMax,
        .SizeEm    = {static_cast<float>(boundsWidth), static_cast<float>(boundsHeight)},
        .BearingEm = {static_cast<float>(bounds.l), static_cast<float>(bounds.b)},
        .IsBlank   = false,
    };
    return &_cache.emplace(key, entry).first->second;
}

} // namespace ImFrame::Internal
