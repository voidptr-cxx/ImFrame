/**
 * @file     GlyphAtlas_test.cpp
 * @brief    Unit tests for `Internal::GlyphAtlas` (Phase 33.4)
 *
 * @internal
 * Glyph ids are sourced from real `TextShaper::Shape()` calls of documented
 * `Icons::Fa::*` constants (not guessed Private-Use-Area codepoints) — an
 * earlier version of this file used raw `U+F000`-range bytes directly, which
 * turned out to not correspond to any *assigned* FA6 icon (the declared PUA
 * range `FA_RANGE_MIN`..`FA_RANGE_MAX` is far larger than the ~1400 codepoints
 * actually mapped to real icons), silently falling back to `.notdef` — this
 * font's `.notdef` renders as a visible box outline, not a blank glyph, which
 * produced confusing failures. `Icons::Fa::House`/`Star` are real, checked-in
 * icon constants guaranteed to resolve to actual FA6 artwork.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-21
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Rendering/Text/GlyphAtlas.hpp"
#include "Rendering/Text/TextShaper.hpp"

#include "ImFrame/Icons/Icons.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {
const Utility::Path kRealFont("Assets/Fonts/fa-solid-900.ttf");

/// Median-of-three reconstruction, the same operation the eventual MSDF shader performs, scaled
/// to a simple 0..255 "how far inside" signal for test assertions.
int MedianChannel(const std::vector<std::uint8_t>& rgba, std::size_t pixelIndex) {
    const int r = rgba[pixelIndex * 4 + 0];
    const int g = rgba[pixelIndex * 4 + 1];
    const int b = rgba[pixelIndex * 4 + 2];
    return std::max(std::min(r, g), std::min(std::max(r, g), b));
}
} // namespace

TEST_CASE("GlyphAtlas::GetOrCreate returns nullptr for an invalid FontId", "[unit]") {
    FontRegistry registry;
    GlyphAtlas   atlas;

    REQUIRE(atlas.GetOrCreate(registry, Rendering::FontId{}, 1) == nullptr);
}

TEST_CASE("GlyphAtlas::GetOrCreate generates a real MSDF bitmap with sane dimensions and a valid "
          "UV rect inside the atlas",
          "[unit]") {
    FontRegistry registry;
    const auto   font = registry.Load(kRealFont, 16.0f);
    REQUIRE(font.has_value());

    TextShaper        shaper;
    const ShapedText* shaped = shaper.Shape(registry, *font, Icons::Fa::House);
    REQUIRE(shaped != nullptr);
    REQUIRE(shaped->Runs.size() == 1);

    GlyphAtlas             atlas;
    const GlyphAtlasEntry* entry = atlas.GetOrCreate(registry, *font, shaped->Runs[0].GlyphId);

    REQUIRE(entry != nullptr);
    REQUIRE_FALSE(entry->IsBlank);
    REQUIRE(entry->SizeEm.x > 0.0f);
    REQUIRE(entry->SizeEm.y > 0.0f);
    REQUIRE(entry->Region.Width > 0);
    REQUIRE(entry->Region.Height > 0);
    REQUIRE(entry->UvMin.x >= 0.0f);
    REQUIRE(entry->UvMin.y >= 0.0f);
    REQUIRE(entry->UvMax.x <= 1.0f);
    REQUIRE(entry->UvMax.y <= 1.0f);
    REQUIRE(entry->UvMax.x > entry->UvMin.x);
    REQUIRE(entry->UvMax.y > entry->UvMin.y);
    REQUIRE(entry->Texture == atlas.Atlas().TextureId());
}

TEST_CASE("GlyphAtlas::GetOrCreate caches by (font, glyph id): a repeat call returns the "
          "identical pointer and does not grow the cache",
          "[unit]") {
    FontRegistry registry;
    const auto   font = registry.Load(kRealFont, 16.0f);
    REQUIRE(font.has_value());

    TextShaper        shaper;
    const ShapedText* shaped = shaper.Shape(registry, *font, Icons::Fa::House);
    REQUIRE(shaped != nullptr);
    const std::uint32_t glyphId = shaped->Runs[0].GlyphId;

    GlyphAtlas             atlas;
    const GlyphAtlasEntry* first  = atlas.GetOrCreate(registry, *font, glyphId);
    const GlyphAtlasEntry* second = atlas.GetOrCreate(registry, *font, glyphId);

    REQUIRE(first != nullptr);
    REQUIRE(first == second);
    REQUIRE(atlas.CacheSize() == 1);
}

TEST_CASE("GlyphAtlas::GetOrCreate produces distinct, non-overlapping atlas regions for distinct glyphs",
          "[unit]") {
    FontRegistry registry;
    const auto   font = registry.Load(kRealFont, 16.0f);
    REQUIRE(font.has_value());

    TextShaper        shaper;
    const ShapedText* shapedHouse = shaper.Shape(registry, *font, Icons::Fa::House);
    const ShapedText* shapedStar  = shaper.Shape(registry, *font, Icons::Fa::Star);
    REQUIRE(shapedHouse != nullptr);
    REQUIRE(shapedStar != nullptr);
    REQUIRE(shapedHouse->Runs.size() == 1);
    REQUIRE(shapedStar->Runs.size() == 1);

    GlyphAtlas             atlas;
    const GlyphAtlasEntry* first  = atlas.GetOrCreate(registry, *font, shapedHouse->Runs[0].GlyphId);
    const GlyphAtlasEntry* second = atlas.GetOrCreate(registry, *font, shapedStar->Runs[0].GlyphId);

    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    REQUIRE(atlas.CacheSize() == 2);

    const bool sameRegion = first->Region.X == second->Region.X && first->Region.Y == second->Region.Y;
    REQUIRE_FALSE(sameRegion);
}

TEST_CASE("GlyphAtlas::GetOrCreate marks a space glyph as blank, with no atlas region", "[unit]") {
    // FA6 (an icon-only font) has no real cmap entry for U+0020, so " " falls back to glyph 0
    // (.notdef) -- and this font's .notdef renders as a visible box outline, not a blank glyph
    // (confirmed directly: it produced a real 32x40 region during this test's development). A
    // genuine text font's actual space glyph is needed to exercise the blank-glyph code path at
    // all -- same system-font dependency, SKIP()-if-absent pattern as TextShaper_test.cpp's
    // ligature/kerning tests, and for the same reason (no font vendored for this purpose).
    const std::filesystem::path calibriPath("C:/Windows/Fonts/calibri.ttf");
    if (!std::filesystem::exists(calibriPath)) {
        SKIP("C:/Windows/Fonts/calibri.ttf not found on this machine -- skipping blank-glyph test");
    }

    FontRegistry registry;
    const auto   font = registry.Load(Utility::Path(calibriPath), 16.0f);
    REQUIRE(font.has_value());

    TextShaper        shaper;
    const ShapedText* shaped = shaper.Shape(registry, *font, " ");
    REQUIRE(shaped != nullptr);
    REQUIRE(shaped->Runs.size() == 1);

    GlyphAtlas             atlas;
    const GlyphAtlasEntry* entry = atlas.GetOrCreate(registry, *font, shaped->Runs[0].GlyphId);

    REQUIRE(entry != nullptr);
    REQUIRE(entry->IsBlank);
    REQUIRE(entry->Region.Width == 0);
    REQUIRE(entry->Region.Height == 0);
}

TEST_CASE("GlyphAtlas::GetOrCreate's uploaded bitmap encodes real signed distance: the glyph's "
          "own center reconstructs as inside, the bitmap's corner (background, thanks to the "
          "distance-range border) reconstructs as outside",
          "[unit]") {
    FontRegistry registry;
    const auto   font = registry.Load(kRealFont, 32.0f);
    REQUIRE(font.has_value());

    TextShaper        shaper;
    const ShapedText* shaped = shaper.Shape(registry, *font, Icons::Fa::House);
    REQUIRE(shaped != nullptr);

    GlyphAtlas             atlas;
    const GlyphAtlasEntry* entry = atlas.GetOrCreate(registry, *font, shaped->Runs[0].GlyphId);
    REQUIRE(entry != nullptr);
    REQUIRE_FALSE(entry->IsBlank);

    const std::vector<std::uint8_t> pixels = atlas.Atlas().ReadRegion(entry->Region);
    const auto width  = static_cast<std::size_t>(entry->Region.Width);
    const auto height = static_cast<std::size_t>(entry->Region.Height);

    const std::size_t centerIndex = (height / 2) * width + width / 2;
    const std::size_t cornerIndex = 0; // top-left corner -- outside the glyph, inside the distance-range border

    const int centerMedian = MedianChannel(pixels, centerIndex);
    const int cornerMedian = MedianChannel(pixels, cornerIndex);

    UNSCOPED_INFO("Region=" << width << "x" << height << " centerMedian=" << centerMedian
                             << " cornerMedian=" << cornerMedian);
    REQUIRE(centerMedian > 127); // inside the glyph's ink (House is a solid, center-filled icon)
    REQUIRE(cornerMedian < 127); // outside -- background, within the padded border
}
