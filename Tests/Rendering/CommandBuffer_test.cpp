/**
 * @file     CommandBuffer_test.cpp
 * @brief    Unit tests for Rendering::CommandBuffer (Phase 31.1)
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-16
 * @version  3.0.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Rendering/CommandBuffer.hpp"

using namespace ImFrame::Rendering;

TEST_CASE("CommandBuffer: pushing 1000 commands preserves count and variant types", "[unit]") {
    CommandBuffer buffer;

    for (int i = 0; i < 1000; ++i) {
        switch (i % 5) {
            case 0: buffer.Push(DrawRect{.Position = {static_cast<float>(i), 0.0f}}); break;
            case 1: buffer.Push(DrawText{.Text = std::to_string(i)}); break;
            case 2: buffer.Push(DrawImage{.Texture = TextureId(static_cast<std::uint64_t>(i))}); break;
            case 3: buffer.Push(PushClipRect{.Size = {10.0f, 10.0f}}); break;
            default: buffer.Push(PopClipRect{}); break;
        }
    }

    REQUIRE(buffer.Size() == 1000);
    REQUIRE_FALSE(buffer.Empty());

    int index = 0;
    for (const Command& cmd : buffer) {
        switch (index % 5) {
            case 0:
                REQUIRE(std::holds_alternative<DrawRect>(cmd));
                REQUIRE(std::get<DrawRect>(cmd).Position.x == static_cast<float>(index));
                break;
            case 1:
                REQUIRE(std::holds_alternative<DrawText>(cmd));
                REQUIRE(std::get<DrawText>(cmd).Text == std::to_string(index));
                break;
            case 2:
                REQUIRE(std::holds_alternative<DrawImage>(cmd));
                REQUIRE(std::get<DrawImage>(cmd).Texture == TextureId(static_cast<std::uint64_t>(index)));
                break;
            case 3:
                REQUIRE(std::holds_alternative<PushClipRect>(cmd));
                break;
            default:
                REQUIRE(std::holds_alternative<PopClipRect>(cmd));
                break;
        }
        ++index;
    }
}

TEST_CASE("CommandBuffer: Reset() clears commands but retains allocated capacity", "[unit]") {
    CommandBuffer buffer;

    for (int i = 0; i < 500; ++i) { buffer.Push(DrawRect{}); }
    REQUIRE(buffer.Size() == 500);
    const std::size_t capacityAfterFirstFrame = buffer.Capacity();
    REQUIRE(capacityAfterFirstFrame >= 500);

    buffer.Reset();
    REQUIRE(buffer.Size() == 0);
    REQUIRE(buffer.Empty());
    REQUIRE(buffer.Capacity() == capacityAfterFirstFrame); // Reset() must not deallocate

    // Second frame with the same command count must not grow capacity (no allocation).
    for (int i = 0; i < 500; ++i) { buffer.Push(DrawRect{}); }
    REQUIRE(buffer.Size() == 500);
    REQUIRE(buffer.Capacity() == capacityAfterFirstFrame);
}

TEST_CASE("CommandBuffer: TextureId/FontId equality and validity", "[unit]") {
    REQUIRE_FALSE(TextureId{}.IsValid());
    REQUIRE(TextureId(42).IsValid());
    REQUIRE(TextureId(42) == TextureId(42));
    REQUIRE_FALSE(TextureId(1) == TextureId(2));

    REQUIRE_FALSE(FontId{}.IsValid());
    REQUIRE(FontId(7).IsValid());
    REQUIRE(FontId(7) == FontId(7));
}

TEST_CASE("CommandBuffer: CornerRadii::All sets all four corners equally", "[unit]") {
    constexpr CornerRadii radii = CornerRadii::All(4.0f);
    REQUIRE(radii.TopLeft == 4.0f);
    REQUIRE(radii.TopRight == 4.0f);
    REQUIRE(radii.BottomRight == 4.0f);
    REQUIRE(radii.BottomLeft == 4.0f);
}
