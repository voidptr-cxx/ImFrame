/**
 * @file     Layout_test.cpp
 * @brief    Unit tests for the pure flex-layout math in `src/Tree/Layout.hpp`
 *
 * Pure math tests — no ImGui context required.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tree/Layout.hpp"

using namespace ImFrame::Internal;
using Catch::Matchers::WithinAbs;

static constexpr float kEps = 1e-4f;

// ─── DistributeFlexFactors ────────────────────────────────────────────────────

TEST_CASE("DistributeFlexFactors: two Expanded(1) children split available space equally", "[unit]") {
    auto sizes = DistributeFlexFactors(200.0f, 0.0f, {1, 1}, {0.0f, 0.0f});
    REQUIRE(sizes.size() == 2);
    REQUIRE_THAT(sizes[0], WithinAbs(100.0f, kEps));
    REQUIRE_THAT(sizes[1], WithinAbs(100.0f, kEps));
}

TEST_CASE("DistributeFlexFactors: factors 1 and 2 split leftover space 1:2", "[unit]") {
    auto sizes = DistributeFlexFactors(300.0f, 0.0f, {1, 2}, {0.0f, 0.0f});
    REQUIRE_THAT(sizes[0], WithinAbs(100.0f, kEps));
    REQUIRE_THAT(sizes[1], WithinAbs(200.0f, kEps));
}

TEST_CASE("DistributeFlexFactors: fixed (factor 0) children keep their intrinsic size", "[unit]") {
    // 200 available, one fixed child at 50, one flexible child takes the rest.
    auto sizes = DistributeFlexFactors(200.0f, 0.0f, {0, 1}, {50.0f, 0.0f});
    REQUIRE_THAT(sizes[0], WithinAbs(50.0f, kEps));
    REQUIRE_THAT(sizes[1], WithinAbs(150.0f, kEps));
}

TEST_CASE("DistributeFlexFactors: gap is subtracted from leftover space before distribution", "[unit]") {
    // 220 available, gap 20 between the two children -> 200 left to split equally.
    auto sizes = DistributeFlexFactors(220.0f, 20.0f, {1, 1}, {0.0f, 0.0f});
    REQUIRE_THAT(sizes[0], WithinAbs(100.0f, kEps));
    REQUIRE_THAT(sizes[1], WithinAbs(100.0f, kEps));
}

TEST_CASE("DistributeFlexFactors: no flexible children leaves fixed sizes untouched", "[unit]") {
    auto sizes = DistributeFlexFactors(500.0f, 0.0f, {0, 0}, {30.0f, 40.0f});
    REQUIRE_THAT(sizes[0], WithinAbs(30.0f, kEps));
    REQUIRE_THAT(sizes[1], WithinAbs(40.0f, kEps));
}

TEST_CASE("DistributeFlexFactors: leftover space never goes negative", "[unit]") {
    // Fixed children alone exceed available space; flexible child gets 0, not negative.
    auto sizes = DistributeFlexFactors(50.0f, 0.0f, {0, 1}, {80.0f, 0.0f});
    REQUIRE_THAT(sizes[0], WithinAbs(80.0f, kEps));
    REQUIRE_THAT(sizes[1], WithinAbs(0.0f, kEps));
}

// ─── ComputeMainAxisOffsets ───────────────────────────────────────────────────

TEST_CASE("ComputeMainAxisOffsets: Start packs children from the leading edge", "[unit]") {
    auto offsets = ComputeMainAxisOffsets(FlexMainAlign::Start, 300.0f, 10.0f, {50.0f, 50.0f});
    REQUIRE_THAT(offsets[0], WithinAbs(0.0f, kEps));
    REQUIRE_THAT(offsets[1], WithinAbs(60.0f, kEps)); // 50 + gap(10)
}

TEST_CASE("ComputeMainAxisOffsets: End packs children against the trailing edge", "[unit]") {
    auto offsets = ComputeMainAxisOffsets(FlexMainAlign::End, 300.0f, 0.0f, {50.0f, 50.0f});
    // free = 300 - 100 = 200; cursor starts at 200.
    REQUIRE_THAT(offsets[0], WithinAbs(200.0f, kEps));
    REQUIRE_THAT(offsets[1], WithinAbs(250.0f, kEps));
}

TEST_CASE("ComputeMainAxisOffsets: Center centers the content block", "[unit]") {
    auto offsets = ComputeMainAxisOffsets(FlexMainAlign::Center, 300.0f, 0.0f, {50.0f, 50.0f});
    // free = 200; centred start = 100.
    REQUIRE_THAT(offsets[0], WithinAbs(100.0f, kEps));
    REQUIRE_THAT(offsets[1], WithinAbs(150.0f, kEps));
}

TEST_CASE("ComputeMainAxisOffsets: SpaceBetween places first and last items at the edges", "[unit]") {
    auto offsets = ComputeMainAxisOffsets(FlexMainAlign::SpaceBetween, 300.0f, 0.0f, {50.0f, 50.0f, 50.0f});
    REQUIRE_THAT(offsets[0], WithinAbs(0.0f, kEps));
    REQUIRE_THAT(offsets[2] + 50.0f, WithinAbs(300.0f, kEps)); // last item's trailing edge == container width
}

TEST_CASE("ComputeMainAxisOffsets: SpaceBetween with one child places it at the start", "[unit]") {
    auto offsets = ComputeMainAxisOffsets(FlexMainAlign::SpaceBetween, 300.0f, 0.0f, {50.0f});
    REQUIRE_THAT(offsets[0], WithinAbs(0.0f, kEps));
}

TEST_CASE("ComputeMainAxisOffsets: SpaceAround distributes equal space around each child", "[unit]") {
    auto offsets = ComputeMainAxisOffsets(FlexMainAlign::SpaceAround, 200.0f, 0.0f, {50.0f, 50.0f});
    // free = 100; spacing = 50; first child starts at 25 (half-spacing).
    REQUIRE_THAT(offsets[0], WithinAbs(25.0f, kEps));
}

// ─── ComputeCrossAxisOffset ───────────────────────────────────────────────────

TEST_CASE("ComputeCrossAxisOffset: Start aligns to zero", "[unit]") {
    REQUIRE_THAT(ComputeCrossAxisOffset(FlexCrossAlign::Start, 100.0f, 30.0f), WithinAbs(0.0f, kEps));
}

TEST_CASE("ComputeCrossAxisOffset: Center centers the child in the cross axis", "[unit]") {
    REQUIRE_THAT(ComputeCrossAxisOffset(FlexCrossAlign::Center, 100.0f, 30.0f), WithinAbs(35.0f, kEps));
}

TEST_CASE("ComputeCrossAxisOffset: End aligns to the trailing edge", "[unit]") {
    REQUIRE_THAT(ComputeCrossAxisOffset(FlexCrossAlign::End, 100.0f, 30.0f), WithinAbs(70.0f, kEps));
}

TEST_CASE("ComputeCrossAxisOffset: Stretch aligns to zero (child is sized to fill)", "[unit]") {
    REQUIRE_THAT(ComputeCrossAxisOffset(FlexCrossAlign::Stretch, 100.0f, 100.0f), WithinAbs(0.0f, kEps));
}
