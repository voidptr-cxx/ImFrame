/**
 * @file     TextShaper.cpp
 * @brief    Implementation of `TextShaper::Shape()` and its LRU cache
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-20
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "TextShaper.hpp"

#include "ImFrame/Core/Error.hpp"

namespace ImFrame::Internal {

namespace {
// HarfBuzz glyph positions are in 26.6 fixed-point units (matching FreeType's own convention,
// since hb_ft_font_create() derives the font's scale from the FT_Face's currently set pixel size).
constexpr float kFixedToPixels = 1.0f / 64.0f;
} // namespace

std::size_t TextShaper::CacheKeyHash::operator()(const CacheKey& key) const noexcept {
    const std::size_t h1 = std::hash<std::uint32_t>{}(key.Font.Value());
    const std::size_t h2 = std::hash<std::string>{}(key.Text);
    return h1 ^ (h2 + 0x9e3779b9U + (h1 << 6) + (h1 >> 2));
}

TextShaper::TextShaper(std::size_t maxCacheEntries)
    : _buffer(hb_buffer_create()), _maxCacheEntries(maxCacheEntries) {
    IMF_ASSERT(_maxCacheEntries > 0);
}

TextShaper::~TextShaper() {
    if (_buffer != nullptr) { hb_buffer_destroy(_buffer); }
}

const ShapedText* TextShaper::Shape(const FontRegistry& registry, Rendering::FontId font, std::string_view text) {
    const FontFace* face = registry.Get(font);
    if (face == nullptr) { return nullptr; }

    CacheKey key{font, std::string(text)};

    const auto found = _cache.find(key);
    if (found != _cache.end()) {
        Touch(found->second.second);
        return &found->second.first;
    }

    ShapedText shaped = BuildShapedText(*face, text);

    _lruOrder.push_front(key);
    const auto inserted = _cache.emplace(std::move(key), std::make_pair(std::move(shaped), _lruOrder.begin()));

    if (_cache.size() > _maxCacheEntries) {
        Evict();
    }

    return &inserted.first->second.first;
}

ShapedText TextShaper::BuildShapedText(const FontFace& face, std::string_view text) {
    hb_buffer_reset(_buffer);
    hb_buffer_add_utf8(_buffer, text.data(), static_cast<int>(text.size()), 0, -1);
    hb_buffer_guess_segment_properties(_buffer);

    hb_shape(face.HarfBuzzFont(), _buffer, nullptr, 0);

    const unsigned int         glyphCount = hb_buffer_get_length(_buffer);
    const hb_glyph_info_t*     infos      = hb_buffer_get_glyph_infos(_buffer, nullptr);
    const hb_glyph_position_t* positions  = hb_buffer_get_glyph_positions(_buffer, nullptr);

    ShapedText result;
    result.Runs.reserve(glyphCount);

    for (unsigned int i = 0; i < glyphCount; ++i) {
        const auto advanceX = static_cast<float>(positions[i].x_advance) * kFixedToPixels;
        const auto advanceY = static_cast<float>(positions[i].y_advance) * kFixedToPixels;

        result.Runs.push_back(GlyphRun{
            .GlyphId = infos[i].codepoint,
            .Advance = {advanceX, advanceY},
            .Offset  = {static_cast<float>(positions[i].x_offset) * kFixedToPixels,
                        static_cast<float>(positions[i].y_offset) * kFixedToPixels},
            .Cluster = infos[i].cluster,
        });

        result.TotalAdvance.x += advanceX;
        result.TotalAdvance.y += advanceY;
    }

    return result;
}

void TextShaper::Touch(LruList::iterator it) {
    _lruOrder.splice(_lruOrder.begin(), _lruOrder, it);
}

void TextShaper::Evict() {
    const CacheKey& lruKey = _lruOrder.back();
    _cache.erase(lruKey);
    _lruOrder.pop_back();
}

} // namespace ImFrame::Internal
