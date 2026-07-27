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

namespace {

/// Builds the four corner vertices of an axis-aligned quad; `DrawRect`'s radii/stroke are not tessellated in 32.1
/// (no shader consumes this geometry yet — `Rect.glsl` receives per-corner radii directly in a later sub-phase).
[[nodiscard]] std::array<Vertex, 4> QuadCorners(Widgets::Vec2 position, Widgets::Vec2 size, Widgets::Vec4 color,
                                                 Widgets::Vec2 uvMin, Widgets::Vec2 uvMax) {
    return {
        Vertex{.Position = {position.x, position.y}, .Uv = {uvMin.x, uvMin.y}, .Color = color},
        Vertex{.Position = {position.x + size.x, position.y}, .Uv = {uvMax.x, uvMin.y}, .Color = color},
        Vertex{.Position = {position.x + size.x, position.y + size.y}, .Uv = {uvMax.x, uvMax.y}, .Color = color},
        Vertex{.Position = {position.x, position.y + size.y}, .Uv = {uvMin.x, uvMax.y}, .Color = color},
    };
}

} // namespace

void BatchBuilder::Reset() {
    _batches.clear();
    _stats = BatchStats{};
    _hasOpen = false;
    _openTexture = Rendering::TextureId{};
    _openVertices.clear();
    _openIndices.clear();
}

void BatchBuilder::FlushOpen(BatchFlushReason reason) {
    if (!_hasOpen) { return; }

    Batch batch;
    batch.Kind = _openKind;
    batch.Texture = _openTexture;
    batch.Vertices = std::move(_openVertices);
    batch.Indices = std::move(_openIndices);

    _stats.DrawCallCount += 1;
    _stats.VertexCount += static_cast<std::uint32_t>(batch.Vertices.size());
    _stats.FlushReasonCounts[static_cast<std::size_t>(reason)] += 1;

    _batches.push_back(std::move(batch));

    _hasOpen = false;
    _openVertices.clear();
    _openIndices.clear();
}

void BatchBuilder::AppendQuad(BatchKind kind, Rendering::TextureId texture, const std::array<Vertex, 4>& corners) {
    const bool kindChanged = _hasOpen && _openKind != kind;
    const bool textureChanged = _hasOpen && !(_openTexture == texture);

    if (kindChanged) {
        FlushOpen(BatchFlushReason::CommandTypeChange);
    } else if (textureChanged) {
        FlushOpen(BatchFlushReason::TextureChange);
    } else if (_hasOpen && _openVertices.size() + 4 > kMaxBatchVertices) {
        FlushOpen(BatchFlushReason::BufferFull);
    }

    if (!_hasOpen) {
        _hasOpen = true;
        _openKind = kind;
        _openTexture = texture;
    }

    const auto base = static_cast<std::uint32_t>(_openVertices.size());
    for (const Vertex& v : corners) { _openVertices.push_back(v); }
    // Two triangles per quad, matching standard CCW-wound UI-quad indexing.
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
                    const auto corners =
                        QuadCorners(cmd.Position, cmd.Size, cmd.FillColor, {0.0f, 0.0f}, {1.0f, 1.0f});
                    AppendQuad(BatchKind::Rect, Rendering::TextureId{}, corners);
                } else if constexpr (std::is_same_v<T, Rendering::DrawImage>) {
                    const auto corners = QuadCorners(cmd.Position, cmd.Size, cmd.TintColor, cmd.UvMin, cmd.UvMax);
                    AppendQuad(BatchKind::Image, cmd.Texture, corners);
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
