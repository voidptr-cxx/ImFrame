/**
 * @file     ClipStack_test.cpp
 * @brief    Unit tests for Internal::ClipStack (Phase 32.1)
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <catch2/catch_test_macros.hpp>

#include "Rendering/Renderers/ClipStack.hpp"

using namespace ImFrame;
using namespace ImFrame::Internal;

TEST_CASE("ClipStack: starts empty", "[unit]") {
    ClipStack clips;
    REQUIRE(clips.Empty());
    REQUIRE(clips.Depth() == 0);
    REQUIRE(clips.CurrentKind() == ClipKind::AxisAligned);
}

TEST_CASE("ClipStack: a single Push becomes the effective region unchanged", "[unit]") {
    ClipStack clips;
    clips.Push(Rendering::PushClipRect{.Position = {10.0f, 20.0f}, .Size = {100.0f, 50.0f}});

    REQUIRE_FALSE(clips.Empty());
    REQUIRE(clips.Depth() == 1);
    REQUIRE(clips.Current().Position.x == 10.0f);
    REQUIRE(clips.Current().Position.y == 20.0f);
    REQUIRE(clips.Current().Size.x == 100.0f);
    REQUIRE(clips.Current().Size.y == 50.0f);
}

TEST_CASE("ClipStack: a nested Push intersects with the parent region", "[unit]") {
    ClipStack clips;
    clips.Push(Rendering::PushClipRect{.Position = {0.0f, 0.0f}, .Size = {100.0f, 100.0f}});
    clips.Push(Rendering::PushClipRect{.Position = {50.0f, 50.0f}, .Size = {100.0f, 100.0f}});

    REQUIRE(clips.Depth() == 2);
    REQUIRE(clips.Current().Position.x == 50.0f);
    REQUIRE(clips.Current().Position.y == 50.0f);
    REQUIRE(clips.Current().Size.x == 50.0f);
    REQUIRE(clips.Current().Size.y == 50.0f);
}

TEST_CASE("ClipStack: a nested Push with no overlap collapses to zero size, not negative", "[unit]") {
    ClipStack clips;
    clips.Push(Rendering::PushClipRect{.Position = {0.0f, 0.0f}, .Size = {10.0f, 10.0f}});
    clips.Push(Rendering::PushClipRect{.Position = {100.0f, 100.0f}, .Size = {10.0f, 10.0f}});

    REQUIRE(clips.Current().Size.x == 0.0f);
    REQUIRE(clips.Current().Size.y == 0.0f);
}

TEST_CASE("ClipStack: Pop restores the previous effective region", "[unit]") {
    ClipStack clips;
    clips.Push(Rendering::PushClipRect{.Position = {0.0f, 0.0f}, .Size = {100.0f, 100.0f}});
    clips.Push(Rendering::PushClipRect{.Position = {50.0f, 50.0f}, .Size = {100.0f, 100.0f}});
    clips.Pop();

    REQUIRE(clips.Depth() == 1);
    REQUIRE(clips.Current().Position.x == 0.0f);
    REQUIRE(clips.Current().Size.x == 100.0f);
}

TEST_CASE("ClipStack: an axis-aligned Push reports ClipKind::AxisAligned", "[unit]") {
    ClipStack clips;
    clips.Push(Rendering::PushClipRect{.Position = {0.0f, 0.0f}, .Size = {10.0f, 10.0f}, .CornerRadius = 0.0f});
    REQUIRE(clips.CurrentKind() == ClipKind::AxisAligned);
}

TEST_CASE("ClipStack: a rounded Push reports ClipKind::Rounded and this propagates to children", "[unit]") {
    ClipStack clips;
    clips.Push(Rendering::PushClipRect{.Position = {0.0f, 0.0f}, .Size = {10.0f, 10.0f}, .CornerRadius = 4.0f});
    REQUIRE(clips.CurrentKind() == ClipKind::Rounded);

    clips.Push(Rendering::PushClipRect{.Position = {1.0f, 1.0f}, .Size = {5.0f, 5.0f}, .CornerRadius = 0.0f});
    REQUIRE(clips.CurrentKind() == ClipKind::Rounded); // still rounded — an ancestor is rounded
}

TEST_CASE("ClipStack: Reset clears the stack back to empty", "[unit]") {
    ClipStack clips;
    clips.Push(Rendering::PushClipRect{.Size = {10.0f, 10.0f}});
    clips.Push(Rendering::PushClipRect{.Size = {5.0f, 5.0f}});
    clips.Reset();

    REQUIRE(clips.Empty());
    REQUIRE(clips.CurrentKind() == ClipKind::AxisAligned);
}
