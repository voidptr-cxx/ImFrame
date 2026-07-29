/**
 * @file     BatchBuilder.cpp
 * @brief    `BatchBuilder` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "BatchBuilder.hpp"

#include <type_traits>
#include <utility>

namespace ImFrame::Internal {

void BatchBuilder::Reset() {
    _batches.clear();
    _stats = BatchStats{};
    _hasOpen = false;
    _openTexture = Rendering::TextureId{};
    _openRectVertices.clear();
    _openImageVertices.clear();
    _openIndices.clear();
}

std::size_t BatchBuilder::OpenVertexCount() const noexcept {
    return _openKind == BatchKind::Rect ? _openRectVertices.size() : _openImageVertices.size();
}

void BatchBuilder::FlushOpen(BatchFlushReason reason) {
    if (!_hasOpen) { return; }

    Batch batch;
    batch.Kind = _openKind;
    batch.Texture = _openTexture;
    batch.Indices = std::move(_openIndices);

    std::size_t vertexCount = 0;
    if (_openKind == BatchKind::Rect) {
        vertexCount = _openRectVertices.size();
        batch.Vertices = std::move(_openRectVertices);
    } else {
        vertexCount = _openImageVertices.size();
        batch.Vertices = std::move(_openImageVertices);
    }

    _stats.DrawCallCount += 1;
    _stats.VertexCount += static_cast<std::uint32_t>(vertexCount);
    _stats.FlushReasonCounts[static_cast<std::size_t>(reason)] += 1;

    _batches.push_back(std::move(batch));

    _hasOpen = false;
    _openRectVertices.clear();
    _openImageVertices.clear();
    _openIndices.clear();
}

void BatchBuilder::BeginBatch(BatchKind kind, Rendering::TextureId texture) {
    _hasOpen = true;
    _openKind = kind;
    _openTexture = texture;
}

void BatchBuilder::AppendRect(const Rendering::DrawRect& cmd) {
    const bool kindChanged = _hasOpen && _openKind != BatchKind::Rect;
    const bool textureChanged = _hasOpen && !(_openTexture == Rendering::TextureId{});

    if (kindChanged) {
        FlushOpen(BatchFlushReason::CommandTypeChange);
    } else if (textureChanged) {
        // Rect batches never carry a texture — this only trips if a prior Rect batch was somehow
        // opened with a non-default texture, which AppendRect itself never does; kept for symmetry
        // with AppendImage's equivalent check and as a defensive invariant.
        FlushOpen(BatchFlushReason::TextureChange);
    } else if (_hasOpen && OpenVertexCount() + 4 > kMaxBatchVertices) {
        FlushOpen(BatchFlushReason::BufferFull);
    }

    if (!_hasOpen) { BeginBatch(BatchKind::Rect, Rendering::TextureId{}); }

    const Widgets::Vec2 center{cmd.Position.x + cmd.Size.x * 0.5f, cmd.Position.y + cmd.Size.y * 0.5f};
    const Widgets::Vec2 halfSize{cmd.Size.x * 0.5f, cmd.Size.y * 0.5f};

    const Widgets::Vec2 corners[4] = {
        {cmd.Position.x, cmd.Position.y},
        {cmd.Position.x + cmd.Size.x, cmd.Position.y},
        {cmd.Position.x + cmd.Size.x, cmd.Position.y + cmd.Size.y},
        {cmd.Position.x, cmd.Position.y + cmd.Size.y},
    };

    const auto base = static_cast<std::uint32_t>(_openRectVertices.size());
    for (const Widgets::Vec2& corner : corners) {
        _openRectVertices.push_back(RectVertex{
            .Position = corner,
            .Local = {corner.x - center.x, corner.y - center.y},
            .HalfSize = halfSize,
            .Radii = cmd.Radii,
            .FillColor = cmd.FillColor,
            .StrokeColor = cmd.StrokeColor,
            .StrokeWidth = cmd.StrokeWidth,
        });
    }

    _openIndices.push_back(base + 0);
    _openIndices.push_back(base + 1);
    _openIndices.push_back(base + 2);
    _openIndices.push_back(base + 0);
    _openIndices.push_back(base + 2);
    _openIndices.push_back(base + 3);
}

void BatchBuilder::AppendImage(const Rendering::DrawImage& cmd) {
    const bool kindChanged = _hasOpen && _openKind != BatchKind::Image;
    const bool textureChanged = _hasOpen && !(_openTexture == cmd.Texture);

    if (kindChanged) {
        FlushOpen(BatchFlushReason::CommandTypeChange);
    } else if (textureChanged) {
        FlushOpen(BatchFlushReason::TextureChange);
    } else if (_hasOpen && OpenVertexCount() + 4 > kMaxBatchVertices) {
        FlushOpen(BatchFlushReason::BufferFull);
    }

    if (!_hasOpen) { BeginBatch(BatchKind::Image, cmd.Texture); }

    const Widgets::Vec2 center{cmd.Position.x + cmd.Size.x * 0.5f, cmd.Position.y + cmd.Size.y * 0.5f};
    const Widgets::Vec2 halfSize{cmd.Size.x * 0.5f, cmd.Size.y * 0.5f};

    const Widgets::Vec2 corners[4] = {
        {cmd.Position.x, cmd.Position.y},
        {cmd.Position.x + cmd.Size.x, cmd.Position.y},
        {cmd.Position.x + cmd.Size.x, cmd.Position.y + cmd.Size.y},
        {cmd.Position.x, cmd.Position.y + cmd.Size.y},
    };
    const Widgets::Vec2 uvs[4] = {
        {cmd.UvMin.x, cmd.UvMin.y},
        {cmd.UvMax.x, cmd.UvMin.y},
        {cmd.UvMax.x, cmd.UvMax.y},
        {cmd.UvMin.x, cmd.UvMax.y},
    };

    const auto base = static_cast<std::uint32_t>(_openImageVertices.size());
    for (int i = 0; i < 4; ++i) {
        _openImageVertices.push_back(ImageVertex{
            .Position = corners[i],
            .Local = {corners[i].x - center.x, corners[i].y - center.y},
            .HalfSize = halfSize,
            .Radii = cmd.Radii,
            .Uv = uvs[i],
            .TintColor = cmd.TintColor,
        });
    }

    _openIndices.push_back(base + 0);
    _openIndices.push_back(base + 1);
    _openIndices.push_back(base + 2);
    _openIndices.push_back(base + 0);
    _openIndices.push_back(base + 2);
    _openIndices.push_back(base + 3);
}

void BatchBuilder::Build(const Rendering::CommandBuffer& buffer) {
    Reset();

    for (const Rendering::Command& command : buffer) {
        std::visit(
            [this](const auto& cmd) {
                using T = std::decay_t<decltype(cmd)>;

                if constexpr (std::is_same_v<T, Rendering::DrawRect>) {
                    AppendRect(cmd);
                } else if constexpr (std::is_same_v<T, Rendering::DrawImage>) {
                    AppendImage(cmd);
                } else if constexpr (std::is_same_v<T, Rendering::DrawPath> || std::is_same_v<T, Rendering::DrawText> ||
                                      std::is_same_v<T, Rendering::DrawShadow>) {
                    FlushOpen(BatchFlushReason::NonBatchable);
                } else if constexpr (std::is_same_v<T, Rendering::PushClipRect> ||
                                      std::is_same_v<T, Rendering::PopClipRect>) {
                    FlushOpen(BatchFlushReason::ClipRectChange);
                } else if constexpr (std::is_same_v<T, Rendering::PushOpacityLayer> ||
                                      std::is_same_v<T, Rendering::PushBlendLayer> ||
                                      std::is_same_v<T, Rendering::PopLayer>) {
                    FlushOpen(BatchFlushReason::LayerChange);
                }
            },
            command);
    }

    FlushOpen(BatchFlushReason::EndOfBuffer);
}

} // namespace ImFrame::Internal
