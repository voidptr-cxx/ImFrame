/**
 * @file     TextShaper_test.cpp
 * @brief    Unit tests for `Internal::TextShaper` (Phase 33.3)
 *
 * @internal
 * The only vendored font (`Assets/Fonts/fa-solid-900.ttf`, FA6 solid icons) has
 * no ligature or kerning tables — it is a symbol font, one glyph per
 * Private-Use-Area codepoint — so it is enough to test basic shaping mechanics
 * (glyph count, non-zero advances, caching, invalid-id handling) but not the
 * ligature/kerning behaviour the proposal specifically calls out. Those two
 * tests reference `C:/Windows/Fonts/calibri.ttf` by absolute path — a real
 * text font with both features, confirmed present on this development
 * machine — and `SKIP()` gracefully if it is absent, the same pattern
 * `IconFont_test.cpp` already uses for its own vendored-asset dependency. This
 * project does not fetch or vendor unreviewed third-party font files, so no
 * ligature-capable font was added to `Assets/Fonts/` for this sub-phase.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-20
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Rendering/Text/TextShaper.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {
const Utility::Path kRealFont("Assets/Fonts/fa-solid-900.ttf");
const std::filesystem::path kCalibriPath("C:/Windows/Fonts/calibri.ttf");
} // namespace

TEST_CASE("TextShaper::Shape returns nullptr for an invalid FontId", "[unit]") {
    FontRegistry registry;
    TextShaper   shaper;

    REQUIRE(shaper.Shape(registry, Rendering::FontId{}, "hello") == nullptr);
}

TEST_CASE("TextShaper::Shape on empty text returns an empty, non-null ShapedText", "[unit]") {
    FontRegistry registry;
    const auto   font = registry.Load(kRealFont, 16.0f);
    REQUIRE(font.has_value());

    TextShaper       shaper;
    const ShapedText* shaped = shaper.Shape(registry, *font, "");

    REQUIRE(shaped != nullptr);
    REQUIRE(shaped->Runs.empty());
    REQUIRE(shaped->TotalAdvance.x == 0.0f);
    REQUIRE(shaped->TotalAdvance.y == 0.0f);
}

TEST_CASE("TextShaper::Shape produces one glyph per codepoint for a non-ligating icon font, "
          "each with a positive advance",
          "[unit]") {
    FontRegistry registry;
    const auto   font = registry.Load(kRealFont, 16.0f);
    REQUIRE(font.has_value());

    TextShaper shaper;
    // Three distinct FA6 Private-Use-Area codepoints, UTF-8 encoded: U+F000, U+F001, U+F002.
    const ShapedText* shaped = shaper.Shape(registry, *font, "\xef\x80\x80\xef\x80\x81\xef\x80\x82");

    REQUIRE(shaped != nullptr);
    REQUIRE(shaped->Runs.size() == 3);
    for (const GlyphRun& run : shaped->Runs) {
        REQUIRE(run.Advance.x > 0.0f);
    }
    REQUIRE(shaped->TotalAdvance.x > 0.0f);
}

TEST_CASE("TextShaper::Shape caches by (font, text): a repeat call returns the identical pointer", "[unit]") {
    FontRegistry registry;
    const auto   font = registry.Load(kRealFont, 16.0f);
    REQUIRE(font.has_value());

    TextShaper shaper;
    const ShapedText* first  = shaper.Shape(registry, *font, "cached text");
    const ShapedText* second = shaper.Shape(registry, *font, "cached text");

    REQUIRE(first != nullptr);
    REQUIRE(first == second);
    REQUIRE(shaper.CacheSize() == 1);
}

TEST_CASE("TextShaper::Shape evicts the least-recently-used entry once the cache is full", "[unit]") {
    FontRegistry registry;
    const auto   font = registry.Load(kRealFont, 16.0f);
    REQUIRE(font.has_value());

    TextShaper shaper(2); // capacity 2, deliberately small to exercise eviction directly

    const ShapedText* pAaa1 = shaper.Shape(registry, *font, "AAA");
    (void)shaper.Shape(registry, *font, "BBB");
    REQUIRE(shaper.CacheSize() == 2);

    // Touch AAA again: AAA becomes most-recently-used, BBB becomes least-recently-used.
    const ShapedText* pAaa2 = shaper.Shape(registry, *font, "AAA");
    REQUIRE(pAaa2 == pAaa1); // still a cache hit -- confirms AAA wasn't evicted by the touch itself

    // A third distinct text must evict the LRU entry (BBB), not AAA.
    (void)shaper.Shape(registry, *font, "CCC");
    REQUIRE(shaper.CacheSize() == 2);

    // AAA surviving (same pointer as originally cached) proves BBB, not AAA, was the one evicted --
    // if the LRU logic evicted AAA instead, this would necessarily be a fresh (different) pointer.
    const ShapedText* pAaa3 = shaper.Shape(registry, *font, "AAA");
    REQUIRE(pAaa3 == pAaa1);
}

TEST_CASE("TextShaper::Shape merges a real 'fi' ligature into a single glyph", "[unit]") {
    if (!std::filesystem::exists(kCalibriPath)) {
        SKIP("C:/Windows/Fonts/calibri.ttf not found on this machine -- skipping ligature test");
    }

    FontRegistry registry;
    const auto   font = registry.Load(Utility::Path(kCalibriPath), 32.0f);
    REQUIRE(font.has_value());

    TextShaper shaper;
    const ShapedText* ligating    = shaper.Shape(registry, *font, "fi");
    const ShapedText* nonLigating = shaper.Shape(registry, *font, "fx");

    REQUIRE(ligating != nullptr);
    REQUIRE(nonLigating != nullptr);
    REQUIRE(ligating->Runs.size() == 1);    // "fi" merges into one ligature glyph
    REQUIRE(nonLigating->Runs.size() == 2); // "fx" does not ligate -- two independent glyphs
}

TEST_CASE("TextShaper::Shape applies kerning: a known kerning pair's combined advance differs "
          "from the sum of its parts shaped independently",
          "[unit]") {
    if (!std::filesystem::exists(kCalibriPath)) {
        SKIP("C:/Windows/Fonts/calibri.ttf not found on this machine -- skipping kerning test");
    }

    FontRegistry registry;
    const auto   font = registry.Load(Utility::Path(kCalibriPath), 32.0f);
    REQUIRE(font.has_value());

    TextShaper shaper;
    const ShapedText* shapedA  = shaper.Shape(registry, *font, "A");
    const ShapedText* shapedV  = shaper.Shape(registry, *font, "V");
    const ShapedText* shapedAV = shaper.Shape(registry, *font, "AV");

    REQUIRE(shapedA != nullptr);
    REQUIRE(shapedV != nullptr);
    REQUIRE(shapedAV != nullptr);

    const float independentSum = shapedA->TotalAdvance.x + shapedV->TotalAdvance.x;
    REQUIRE(std::abs(shapedAV->TotalAdvance.x - independentSum) > 0.01f);
}
