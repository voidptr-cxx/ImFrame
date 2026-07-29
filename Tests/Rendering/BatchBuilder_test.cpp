/**
 * @file     BatchBuilder_test.cpp
 * @brief    Unit tests for Internal::BatchBuilder (Phase 32.1, vertex layout reconciled in Phase 32.3)
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

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Rendering/Renderers/BatchBuilder.hpp"

using namespace ImFrame;
using namespace ImFrame::Internal;
using Catch::Approx;

TEST_CASE("BatchBuilder: adjacent DrawRects with no intervening state change form one batch", "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawRect{.Position = {0.0f, 0.0f}, .Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::DrawRect{.Position = {10.0f, 0.0f}, .Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::DrawRect{.Position = {20.0f, 0.0f}, .Size = {10.0f, 10.0f}});

    BatchBuilder builder;
    builder.Build(buffer);

    REQUIRE(builder.Batches().size() == 1);
    const auto& vertices = std::get<std::vector<RectVertex>>(builder.Batches().front().Vertices);
    REQUIRE(vertices.size() == 12);
    REQUIRE(builder.Batches().front().Indices.size() == 18);
    REQUIRE(builder.Stats().DrawCallCount == 1);
    REQUIRE(builder.Stats().VertexCount == 12);
    REQUIRE(builder.Stats().FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::EndOfBuffer)] == 1);
}

TEST_CASE("BatchBuilder: RectVertex Local/HalfSize/Radii are computed correctly from DrawRect", "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawRect{
        .Position = {10.0f, 20.0f},
        .Size = {40.0f, 20.0f},
        .Radii = {.TopLeft = 1.0f, .TopRight = 2.0f, .BottomRight = 3.0f, .BottomLeft = 4.0f},
        .FillColor = {0.1f, 0.2f, 0.3f, 0.4f},
        .StrokeColor = {0.5f, 0.6f, 0.7f, 0.8f},
        .StrokeWidth = 2.0f,
    });

    BatchBuilder builder;
    builder.Build(buffer);

    const auto& vertices = std::get<std::vector<RectVertex>>(builder.Batches().front().Vertices);
    REQUIRE(vertices.size() == 4);

    // Rect center is (10+20, 20+10) = (30, 30); half-size is (20, 10).
    for (const RectVertex& v : vertices) {
        REQUIRE(v.HalfSize.x == Approx(20.0f));
        REQUIRE(v.HalfSize.y == Approx(10.0f));
        REQUIRE(v.Local.x == Approx(v.Position.x - 30.0f));
        REQUIRE(v.Local.y == Approx(v.Position.y - 30.0f));
        REQUIRE(v.Radii.TopLeft == Approx(1.0f));
        REQUIRE(v.Radii.TopRight == Approx(2.0f));
        REQUIRE(v.Radii.BottomRight == Approx(3.0f));
        REQUIRE(v.Radii.BottomLeft == Approx(4.0f));
        REQUIRE(v.StrokeWidth == Approx(2.0f));
    }

    // First vertex is the top-left corner: Position (10, 20), Local (-20, -10).
    REQUIRE(vertices[0].Position.x == Approx(10.0f));
    REQUIRE(vertices[0].Position.y == Approx(20.0f));
    REQUIRE(vertices[0].Local.x == Approx(-20.0f));
    REQUIRE(vertices[0].Local.y == Approx(-10.0f));
}

TEST_CASE("BatchBuilder: ImageVertex carries UV/tint/rounding independently per corner", "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawImage{
        .Position = {0.0f, 0.0f},
        .Size = {10.0f, 10.0f},
        .Texture = Rendering::TextureId(3),
        .UvMin = {0.25f, 0.0f},
        .UvMax = {1.0f, 0.75f},
        .TintColor = {1.0f, 0.5f, 0.25f, 1.0f},
    });

    BatchBuilder builder;
    builder.Build(buffer);

    const auto& vertices = std::get<std::vector<ImageVertex>>(builder.Batches().front().Vertices);
    REQUIRE(vertices.size() == 4);
    REQUIRE(vertices[0].Uv.x == Approx(0.25f)); // top-left uses UvMin
    REQUIRE(vertices[0].Uv.y == Approx(0.0f));
    REQUIRE(vertices[2].Uv.x == Approx(1.0f)); // bottom-right uses UvMax
    REQUIRE(vertices[2].Uv.y == Approx(0.75f));
    REQUIRE(vertices[0].TintColor.x == Approx(1.0f));
    REQUIRE(vertices[0].TintColor.y == Approx(0.5f));
}

TEST_CASE("BatchBuilder: a DrawImage after a DrawRect run flushes on command-type change", "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::DrawImage{.Size = {10.0f, 10.0f}, .Texture = Rendering::TextureId(7)});

    BatchBuilder builder;
    builder.Build(buffer);

    REQUIRE(builder.Batches().size() == 2);
    REQUIRE(builder.Batches()[0].Kind == BatchKind::Rect);
    REQUIRE(builder.Batches()[1].Kind == BatchKind::Image);
    REQUIRE(builder.Stats().FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::CommandTypeChange)] == 1);
}

TEST_CASE("BatchBuilder: two DrawImages with different textures flush on texture change", "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawImage{.Size = {10.0f, 10.0f}, .Texture = Rendering::TextureId(1)});
    buffer.Push(Rendering::DrawImage{.Size = {10.0f, 10.0f}, .Texture = Rendering::TextureId(2)});

    BatchBuilder builder;
    builder.Build(buffer);

    REQUIRE(builder.Batches().size() == 2);
    REQUIRE(builder.Stats().FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::TextureChange)] == 1);
}

TEST_CASE("BatchBuilder: PushClipRect/PopClipRect around a rect forces flushes on both sides", "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::PushClipRect{.Size = {100.0f, 100.0f}});
    buffer.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::PopClipRect{});
    buffer.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});

    BatchBuilder builder;
    builder.Build(buffer);

    // Rect / [clip] / Rect / [unclip] / Rect — each DrawRect isolated by a clip boundary on either side.
    REQUIRE(builder.Batches().size() == 3);
    REQUIRE(builder.Stats().FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::ClipRectChange)] == 2);
}

TEST_CASE("BatchBuilder: DrawText between two DrawRects flushes as non-batchable and opens no batch of its own",
          "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::DrawText{.Text = "hi"});
    buffer.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});

    BatchBuilder builder;
    builder.Build(buffer);

    REQUIRE(builder.Batches().size() == 2);
    REQUIRE(builder.Stats().FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::NonBatchable)] == 1);
}

TEST_CASE("BatchBuilder: DrawPath is treated as non-batchable (no CPU tesselator/live producer yet)", "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::DrawPath{});
    buffer.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});

    BatchBuilder builder;
    builder.Build(buffer);

    REQUIRE(builder.Batches().size() == 2);
    REQUIRE(builder.Stats().FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::NonBatchable)] == 1);
}

TEST_CASE("BatchBuilder: an empty command buffer produces zero batches", "[unit]") {
    Rendering::CommandBuffer buffer;

    BatchBuilder builder;
    builder.Build(buffer);

    REQUIRE(builder.Batches().empty());
    REQUIRE(builder.Stats().DrawCallCount == 0);
}

TEST_CASE("BatchBuilder: Build() is idempotent across repeated calls with different inputs", "[unit]") {
    BatchBuilder builder;

    Rendering::CommandBuffer first;
    first.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});
    first.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});
    builder.Build(first);
    REQUIRE(builder.Batches().size() == 1);

    Rendering::CommandBuffer second;
    second.Push(Rendering::DrawRect{.Size = {10.0f, 10.0f}});
    second.Push(Rendering::DrawImage{.Size = {10.0f, 10.0f}});
    builder.Build(second);
    REQUIRE(builder.Batches().size() == 2);
}

TEST_CASE("BatchBuilder: exceeding kMaxBatchVertices force-flushes with BufferFull", "[unit]") {
    Rendering::CommandBuffer buffer;
    const std::uint32_t quadsToOverflow = BatchBuilder::kMaxBatchVertices / 4 + 1;
    for (std::uint32_t i = 0; i < quadsToOverflow; ++i) {
        buffer.Push(Rendering::DrawRect{.Size = {1.0f, 1.0f}});
    }

    BatchBuilder builder;
    builder.Build(buffer);

    REQUIRE(builder.Batches().size() == 2);
    REQUIRE(builder.Stats().FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::BufferFull)] == 1);
}
