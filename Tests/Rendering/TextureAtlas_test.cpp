/**
 * @file     TextureAtlas_test.cpp
 * @brief    Unit tests for Internal::TextureAtlas (Phase 32.1)
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Rendering/Renderers/TextureAtlas.hpp"

using namespace ImFrame::Internal;
using Catch::Approx;

TEST_CASE("TextureAtlas: starts at the configured dimensions and a valid TextureId", "[unit]") {
    TextureAtlas atlas;
    REQUIRE(atlas.Width() == TextureAtlas::kDefaultWidth);
    REQUIRE(atlas.Height() == TextureAtlas::kDefaultHeight);
    REQUIRE(atlas.MaxHeight() == TextureAtlas::kDefaultMaxHeight);
    REQUIRE(atlas.TextureId().IsValid());
}

TEST_CASE("TextureAtlas: two instances mint distinct TextureIds", "[unit]") {
    TextureAtlas a;
    TextureAtlas b;
    REQUIRE_FALSE(a.TextureId() == b.TextureId());
}

TEST_CASE("TextureAtlas: sequential Allocs pack left-to-right on the same shelf", "[unit]") {
    TextureAtlas atlas(256, 64, 256);

    auto first = atlas.Alloc(16, 16);
    auto second = atlas.Alloc(16, 16);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(first->X == 0);
    REQUIRE(second->X == 16);
    REQUIRE(first->Y == second->Y);
}

TEST_CASE("TextureAtlas: a region wider than the atlas fails to allocate", "[unit]") {
    TextureAtlas atlas(64, 64, 64);
    REQUIRE_FALSE(atlas.Alloc(128, 16).has_value());
}

TEST_CASE("TextureAtlas: filling a shelf's width starts a new shelf below it", "[unit]") {
    TextureAtlas atlas(32, 32, 256);

    auto first = atlas.Alloc(32, 8);  // fills the first shelf's width exactly
    auto second = atlas.Alloc(16, 8); // must start a new shelf
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(second->Y == first->Y + first->Height);
}

TEST_CASE("TextureAtlas: growth doubles height up to MaxHeight and preserves earlier region coordinates", "[unit]") {
    TextureAtlas atlas(32, 16, 64);

    auto first = atlas.Alloc(32, 16); // fills the initial 32x16 canvas exactly
    REQUIRE(first.has_value());
    REQUIRE(atlas.Height() == 16);

    auto second = atlas.Alloc(32, 16); // forces a grow (16 -> 32)
    REQUIRE(second.has_value());
    REQUIRE(atlas.Height() == 32);
    REQUIRE(first->X == 0);
    REQUIRE(first->Y == 0); // unchanged by the grow

    auto third = atlas.Alloc(32, 16); // forces another grow (32 -> 64, hits MaxHeight)
    REQUIRE(third.has_value());
    REQUIRE(atlas.Height() == 64);

    // 3 shelves of 16 rows used 48 of the 64 rows now available — one more 16-row shelf still fits
    // exactly (48 + 16 == 64) without growing further, since MaxHeight is already reached.
    auto fourth = atlas.Alloc(32, 16);
    REQUIRE(fourth.has_value());
    REQUIRE(atlas.Height() == 64);

    // Now the atlas is genuinely full (4 * 16 == 64 == MaxHeight) — no more room, and it cannot grow further.
    REQUIRE_FALSE(atlas.Alloc(32, 16).has_value());
}

TEST_CASE("TextureAtlas: Upload followed by ReadRegion round-trips pixel data exactly", "[unit]") {
    TextureAtlas atlas(64, 64, 64);
    auto region = atlas.Alloc(4, 2);
    REQUIRE(region.has_value());

    std::vector<std::uint8_t> pixels(4 * 2 * 4);
    for (std::size_t i = 0; i < pixels.size(); ++i) { pixels[i] = static_cast<std::uint8_t>(i); }

    atlas.Upload(*region, pixels);
    REQUIRE(atlas.ReadRegion(*region) == pixels);
}

TEST_CASE("TextureAtlas: growth preserves already-uploaded pixel content", "[unit]") {
    TextureAtlas atlas(16, 8, 32);
    auto region = atlas.Alloc(16, 8); // fills the initial canvas
    REQUIRE(region.has_value());

    std::vector<std::uint8_t> pixels(16 * 8 * 4, 0xAB);
    atlas.Upload(*region, pixels);

    auto forceGrow = atlas.Alloc(16, 8);
    REQUIRE(forceGrow.has_value());
    REQUIRE(atlas.Height() == 16);

    REQUIRE(atlas.ReadRegion(*region) == pixels);
}

TEST_CASE("TextureAtlas: Generation() increments on Upload() and Grow(), and is stable otherwise", "[unit]") {
    TextureAtlas atlas(16, 8, 32);
    REQUIRE(atlas.Generation() == 0);

    auto region = atlas.Alloc(4, 4); // opens the first shelf -- no growth needed yet
    REQUIRE(region.has_value());
    REQUIRE(atlas.Generation() == 0); // Alloc() alone doesn't touch pixels

    std::vector<std::uint8_t> pixels(4 * 4 * 4, 0xCD);
    atlas.Upload(*region, pixels);
    REQUIRE(atlas.Generation() == 1);

    auto forceGrow = atlas.Alloc(16, 8); // needs a new, taller shelf -- forces Grow()
    REQUIRE(forceGrow.has_value());
    REQUIRE(atlas.Height() == 16); // confirms Grow() actually ran
    REQUIRE(atlas.Generation() == 2);
}

TEST_CASE("TextureAtlas: Uv() is normalized to the atlas's current dimensions", "[unit]") {
    TextureAtlas atlas(100, 100, 100);
    auto region = atlas.Alloc(25, 50);
    REQUIRE(region.has_value());

    auto [uvMin, uvMax] = atlas.Uv(*region);
    REQUIRE(uvMin.x == Approx(0.0f));
    REQUIRE(uvMin.y == Approx(0.0f));
    REQUIRE(uvMax.x == Approx(0.25f));
    REQUIRE(uvMax.y == Approx(0.5f));
}
