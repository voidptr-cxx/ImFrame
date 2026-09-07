/**
 * @file     ColorToken_test.cpp
 * @brief    Unit tests for ColorToken, SpacingToken, and RadiusToken
 *
 * @internal
 * These tests are purely compile-time/arithmetic — no ImGui context is required.
 * Tests follow the same pattern as Timer_test.cpp and Config_test.cpp.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-04
 * @version  0.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Theme/ColorToken.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace ImFrame::Theme;
using Catch::Approx;

// ─── ColorToken ───────────────────────────────────────────────────────────────

TEST_CASE("ColorToken stores correct r/g/b/a after construction", "[unit]") {
    constexpr ColorToken c{0.2f, 0.4f, 0.6f, 0.8f};

    static_assert(c.r == 0.2f, "ColorToken r mismatch");
    static_assert(c.g == 0.4f, "ColorToken g mismatch");
    static_assert(c.b == 0.6f, "ColorToken b mismatch");
    static_assert(c.a == 0.8f, "ColorToken a mismatch");

    // Runtime checks confirm the same values survive to execution
    REQUIRE(c.r == Approx(0.2f));
    REQUIRE(c.g == Approx(0.4f));
    REQUIRE(c.b == Approx(0.6f));
    REQUIRE(c.a == Approx(0.8f));
}

TEST_CASE("ColorToken default alpha is 1.0f", "[unit]") {
    constexpr ColorToken c{0.1f, 0.2f, 0.3f};
    static_assert(c.a == 1.0f, "ColorToken default alpha should be 1.0f");
    SUCCEED();
}

TEST_CASE("ColorFromHex converts 0xFF0000 to red", "[unit]") {
    constexpr ColorToken red = ColorFromHex(0xFF0000u);

    static_assert(red.r == 1.0f, "red channel should be 1.0");
    static_assert(red.g == 0.0f, "green channel should be 0.0");
    static_assert(red.b == 0.0f, "blue channel should be 0.0");
    static_assert(red.a == 1.0f, "alpha should default to 1.0");

    REQUIRE(red.r == Approx(1.0f));
    REQUIRE(red.g == Approx(0.0f));
    REQUIRE(red.b == Approx(0.0f));
    REQUIRE(red.a == Approx(1.0f));
}

TEST_CASE("ColorFromHex respects explicit alpha override", "[unit]") {
    constexpr ColorToken semi = ColorFromHex(0xFFFFFFu, 0.5f);

    static_assert(semi.a == 0.5f, "alpha override should be 0.5f");
    REQUIRE(semi.a == Approx(0.5f));
    REQUIRE(semi.r == Approx(1.0f));
}

TEST_CASE("ColorFromHex converts each channel independently", "[unit]") {
    constexpr ColorToken c = ColorFromHex(0x804020u);

    REQUIRE(c.r == Approx(0x80 / 255.0f));
    REQUIRE(c.g == Approx(0x40 / 255.0f));
    REQUIRE(c.b == Approx(0x20 / 255.0f));
}

// ─── SpacingToken ─────────────────────────────────────────────────────────────

TEST_CASE("SpacingToken is constexpr-constructible with defaults", "[unit]") {
    constexpr SpacingToken s;

    static_assert(s.ItemSpacingX   == 8.0f);
    static_assert(s.ItemSpacingY   == 4.0f);
    static_assert(s.WindowPaddingX == 8.0f);
    static_assert(s.WindowPaddingY == 8.0f);
    static_assert(s.FramePaddingX  == 4.0f);
    static_assert(s.FramePaddingY  == 3.0f);
    static_assert(s.IndentSpacing  == 21.0f);

    SUCCEED();
}

TEST_CASE("SpacingToken accepts custom values via designated initialiser", "[unit]") {
    constexpr SpacingToken s{.ItemSpacingX = 12.0f, .ItemSpacingY = 6.0f,
                              .WindowPaddingX = 10.0f, .WindowPaddingY = 10.0f,
                              .FramePaddingX = 5.0f, .FramePaddingY = 4.0f,
                              .IndentSpacing = 18.0f};

    static_assert(s.ItemSpacingX == 12.0f);
    static_assert(s.IndentSpacing == 18.0f);
    SUCCEED();
}

// ─── RadiusToken ──────────────────────────────────────────────────────────────

TEST_CASE("RadiusToken is constexpr-constructible with defaults", "[unit]") {
    constexpr RadiusToken r;

    static_assert(r.Window    == 4.0f);
    static_assert(r.Frame     == 4.0f);
    static_assert(r.Popup     == 4.0f);
    static_assert(r.Scrollbar == 9.0f);

    SUCCEED();
}

TEST_CASE("RadiusToken accepts custom values", "[unit]") {
    constexpr RadiusToken r{.Window = 8.0f, .Frame = 8.0f,
                             .Popup = 8.0f, .Scrollbar = 12.0f};

    static_assert(r.Window    == 8.0f);
    static_assert(r.Scrollbar == 12.0f);
    SUCCEED();
}
