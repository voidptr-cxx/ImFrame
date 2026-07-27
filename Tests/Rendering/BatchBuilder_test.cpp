/**
 * @file     BatchBuilder_test.cpp
 * @brief    Unit tests for Internal::BatchBuilder (Phase 32.1)
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

#include "Rendering/Renderers/BatchBuilder.hpp"

using namespace ImFrame;
using namespace ImFrame::Internal;

TEST_CASE("BatchBuilder: adjacent DrawRects with no intervening state change form one batch", "[unit]") {
    Rendering::CommandBuffer buffer;
    buffer.Push(Rendering::DrawRect{.Position = {0.0f, 0.0f}, .Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::DrawRect{.Position = {10.0f, 0.0f}, .Size = {10.0f, 10.0f}});
    buffer.Push(Rendering::DrawRect{.Position = {20.0f, 0.0f}, .Size = {10.0f, 10.0f}});

    BatchBuilder builder;
    builder.Build(buffer);

    REQUIRE(builder.Batches().size() == 1);
    REQUIRE(builder.Batches().front().Vertices.size() == 12);
    REQUIRE(builder.Batches().front().Indices.size() == 18);
    REQUIRE(builder.Stats().DrawCallCount == 1);
    REQUIRE(builder.Stats().VertexCount == 12);
    REQUIRE(builder.Stats().FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::EndOfBuffer)] == 1);
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
