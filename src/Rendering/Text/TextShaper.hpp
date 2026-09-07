/**
 * @file     TextShaper.hpp
 * @brief    Shapes UTF-8 text with a `FontRegistry`-loaded font into positioned glyphs
 *
 * @internal
 * `hb_buffer_guess_segment_properties()` auto-detects script/language/direction
 * from the buffer's own Unicode content before shaping — this is HarfBuzz's own
 * idiomatic single-run usage and is what gives ligatures, kerning, and correct
 * per-run bidi direction (RTL scripts shape with negative x-advances) "for free."
 * Full paragraph-level bidi *segmentation* across mixed-direction text (splitting
 * one logical string into multiple directional runs before shaping each) is a
 * separate, much larger concern (a real Unicode bidi algorithm implementation)
 * and is out of scope here — `Shape()` treats its whole input as one run.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-20
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "FontRegistry.hpp"

#include "ImFrame/Widgets/Types.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ImFrame::Internal {

/**
 * @struct   GlyphRun
 * @brief    One shaped glyph — which glyph to draw, where to advance the pen, and its offset
 *
 * @since    3.0.0
 */
struct GlyphRun {
    std::uint32_t GlyphId = 0; ///< `hb_codepoint_t` after shaping — a glyph index, not a Unicode codepoint.
    Widgets::Vec2 Advance{};   ///< Pen advance after this glyph, in pixels.
    Widgets::Vec2 Offset{};    ///< Offset from the pen position for this glyph, in pixels (kerning/marks).
    std::uint32_t Cluster = 0; ///< Byte offset into the original UTF-8 string this glyph maps to.
};

/**
 * @struct   ShapedText
 * @brief    The result of shaping one run of text with one font — a sequence of positioned glyphs
 *
 * @since    3.0.0
 */
struct ShapedText {
    std::vector<GlyphRun> Runs;
    Widgets::Vec2         TotalAdvance{}; ///< Sum of every `Runs` entry's `Advance` — the run's total pen movement.
};

/**
 * @class    TextShaper
 * @brief    Shapes text via HarfBuzz, caching results by (font, text) with LRU eviction
 *
 * @internal
 * One `hb_buffer_t` is reused across `Shape()` calls (reset, not recreated, each
 * time) — HarfBuzz's own recommended usage pattern to avoid a buffer allocation
 * per shape. Non-copyable, non-moveable: the returned `ShapedText*` pointers are
 * only meaningful relative to the exact `TextShaper` instance that produced them.
 *
 * @since    3.0.0
 */
class TextShaper {
public:
    /// Default LRU cache capacity. Chosen as "generous for a typical single frame's worth of
    /// distinct on-screen strings, small enough that eviction is exercised under real use rather
    /// than only in a synthetic test" — not derived from a measured workload (none exists yet).
    static constexpr std::size_t kDefaultMaxCacheEntries = 256;

    explicit TextShaper(std::size_t maxCacheEntries = kDefaultMaxCacheEntries);
    ~TextShaper();

    TextShaper(const TextShaper&)            = delete;
    TextShaper& operator=(const TextShaper&) = delete;
    TextShaper(TextShaper&&)                 = delete;
    TextShaper& operator=(TextShaper&&)      = delete;

    /**
     * @brief    Shapes `text` with `font`, reusing a cached result if (`font`, `text`) was shaped before.
     *
     * @param[in]  registry  Registry to resolve `font` against.
     * @param[in]  font      A `FontId` returned by `registry.Load()`.
     * @param[in]  text      UTF-8 encoded text to shape.
     * @return   Pointer to the (possibly cached) `ShapedText`, valid until a later `Shape()`
     *           call on this same instance evicts it from the LRU cache, or `nullptr` if
     *           `font` is not a valid id in `registry`.
     * @throws   Nothing.
     */
    [[nodiscard]] const ShapedText* Shape(const FontRegistry& registry, Rendering::FontId font, std::string_view text);

    /// Number of entries currently cached. Exposed for tests only.
    [[nodiscard]] std::size_t CacheSize() const noexcept { return _cache.size(); }

private:
    struct CacheKey {
        Rendering::FontId Font;
        std::string       Text;

        [[nodiscard]] friend bool operator==(const CacheKey&, const CacheKey&) = default;
    };

    /// Combines FontId + string hashes — FontId has no std::hash specialization, and adding one
    /// globally isn't warranted for this sole internal use (matches FontRegistry's own reasoning
    /// for keying its map by FontId::Value() rather than FontId itself).
    struct CacheKeyHash {
        [[nodiscard]] std::size_t operator()(const CacheKey& key) const noexcept;
    };

    using LruList = std::list<CacheKey>;

    [[nodiscard]] ShapedText BuildShapedText(const FontFace& face, std::string_view text);
    void Touch(LruList::iterator it);
    void Evict();

    hb_buffer_t* _buffer = nullptr;
    std::size_t  _maxCacheEntries;

    LruList _lruOrder; ///< Front = most recently used, back = next to evict.
    std::unordered_map<CacheKey, std::pair<ShapedText, LruList::iterator>, CacheKeyHash> _cache;
};

} // namespace ImFrame::Internal
