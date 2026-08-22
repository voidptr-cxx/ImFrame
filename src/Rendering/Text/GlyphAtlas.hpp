/**
 * @file     GlyphAtlas.hpp
 * @brief    Generates MSDF glyph bitmaps on demand and packs them into a `TextureAtlas`
 *
 * @internal
 * On first encounter with a (font, glyph id) pair, `GetOrCreate()` loads the glyph's outline
 * from the same real `FT_Face` `FontRegistry` already owns (via msdfgen's `adoptFreetypeFont()`
 * — no separate FreeType library instance or duplicate font load), generates a multi-channel +
 * true-distance (MTSDF) bitmap via `msdfgen::generateMTSDF()`, and uploads it into a shared
 * `Rendering::Internal::TextureAtlas` (Phase 32.1 — this is that type's first live producer).
 * Subsequent requests for the same (font, glyph id) hit the cache; msdfgen is never invoked twice
 * for the same glyph.
 *
 * The MSDF source raster density (`kPixelsPerEm`) is fixed and independent of whatever pixel size
 * a `FontRegistry::Load()` call happened to request — that size only affects HarfBuzz's advance
 * metrics (Phase 33.3's `TextShaper`), not glyph bitmap generation. This is the entire point of
 * MSDF: one bitmap, generated once, reconstructs sharp edges at any final on-screen size via the
 * shader's median-of-three + `fwidth()`-based anti-aliasing (a later sub-phase).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-21
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "FontRegistry.hpp"

#include "Rendering/Renderers/TextureAtlas.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace ImFrame::Internal {

/**
 * @struct   GlyphAtlasEntry
 * @brief    UV rect + placement metrics for one glyph's MSDF bitmap inside a `TextureAtlas`
 *
 * @since    3.0.0
 */
struct GlyphAtlasEntry {
    Rendering::TextureId Texture; ///< Which atlas's (conceptual) texture this glyph was uploaded to.
    AtlasRegion   Region{};       ///< Raw pixel rect within `Texture` — default-zero when `IsBlank`.
    Widgets::Vec2 UvMin{};
    Widgets::Vec2 UvMax{};
    Widgets::Vec2 SizeEm{}; ///< Glyph quad size in em units (tight outline bounds + distance-range border).

    /// Bitmap's bottom-left corner, in em units, relative to the pen/baseline origin — using
    /// msdfgen's own Y-up (mathematical) convention. A renderer placing this quad in ImFrame's
    /// top-down screen space (matching every other screen-space `Vec2` in this codebase) must
    /// negate the Y component when computing the quad's final on-screen position.
    Widgets::Vec2 BearingEm{};

    /// `true` for a glyph with no visible ink (space, zero-width joiner, a degenerate outline) —
    /// `UvMin`/`UvMax`/`SizeEm`/`BearingEm` are all default-zero and no atlas region was allocated.
    bool IsBlank = false;
};

/**
 * @class    GlyphAtlas
 * @brief    Generates one MSDF bitmap per distinct (font, glyph id) the first time it's requested
 *
 * @internal
 * Non-copyable, non-moveable — matches `FontRegistry`'s own reasoning (every `GlyphAtlasEntry*` a
 * caller holds implicitly refers to a specific instance's atlas/cache).
 *
 * @since    3.0.0
 */
class GlyphAtlas {
public:
    /// MSDF source raster density, in pixels per em — independent of any font's *loaded* pixel
    /// size (see this class's own file comment). Not derived from a measured quality/memory
    /// tradeoff; a reasonable starting density for a first working version.
    static constexpr double kPixelsPerEm = 32.0;

    /// Distance field range, in *output* pixels — how many pixels around the glyph's tight
    /// outline carry meaningful (non-clamped) distance values. A conventional default for MSDF
    /// text rendering, matching msdfgen's own commonly-used examples.
    static constexpr double kPxRange = 4.0;

    explicit GlyphAtlas(std::uint32_t atlasWidth = TextureAtlas::kDefaultWidth,
                         std::uint32_t atlasHeight = TextureAtlas::kDefaultHeight);

    GlyphAtlas(const GlyphAtlas&)            = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;
    GlyphAtlas(GlyphAtlas&&)                 = delete;
    GlyphAtlas& operator=(GlyphAtlas&&)      = delete;

    /**
     * @brief    Returns the atlas entry for (font, glyphId), generating/uploading it on first use.
     *
     * @param[in]  registry  Registry to resolve `font` against.
     * @param[in]  font      A `FontId` from `registry.Load()`.
     * @param[in]  glyphId   A glyph index as HarfBuzz shaping already resolved it
     *                       (`TextShaper`'s `GlyphRun::GlyphId`) — not a Unicode codepoint.
     * @return   Pointer to the (possibly cached) entry, valid for this `GlyphAtlas`'s lifetime,
     *           or `nullptr` if `font` is invalid, the glyph's outline could not be loaded, or
     *           the atlas is full and cannot grow further.
     * @throws   Nothing.
     */
    [[nodiscard]] const GlyphAtlasEntry* GetOrCreate(const FontRegistry& registry, Rendering::FontId font,
                                                      std::uint32_t glyphId);

    [[nodiscard]] const TextureAtlas& Atlas() const noexcept { return _atlas; }

    /// Number of entries currently cached. Exposed for tests only.
    [[nodiscard]] std::size_t CacheSize() const noexcept { return _cache.size(); }

private:
    struct CacheKey {
        Rendering::FontId Font;
        std::uint32_t     GlyphId = 0;

        [[nodiscard]] friend bool operator==(const CacheKey&, const CacheKey&) = default;
    };

    /// See `TextShaper`/`FontRegistry`'s identical reasoning for not adding a global `std::hash<FontId>`.
    struct CacheKeyHash {
        [[nodiscard]] std::size_t operator()(const CacheKey& key) const noexcept;
    };

    TextureAtlas _atlas;
    std::unordered_map<CacheKey, GlyphAtlasEntry, CacheKeyHash> _cache;
};

} // namespace ImFrame::Internal
