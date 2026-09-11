/**
 * @file     BatchBuilder.cpp
 * @brief    `BatchBuilder` implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
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
    _openTextVertices.clear();
    _openIndices.clear();
}

std::size_t BatchBuilder::OpenVertexCount() const noexcept {
    switch (_openKind) {
        case BatchKind::Rect:  return _openRectVertices.size();
        case BatchKind::Image: return _openImageVertices.size();
        case BatchKind::Text:  return _openTextVertices.size();
        case BatchKind::Shadow:
        case BatchKind::Layer:
        case BatchKind::BackdropBlur:
            // Unreachable: AppendShadow()/AppendLayerMarker()/AppendBackdropBlur() never call
            // BeginBatch(), so _openKind is never one of these. Cases kept for exhaustiveness.
            return 0;
    }
    return 0;
}

void BatchBuilder::FlushOpen(BatchFlushReason reason) {
    if (!_hasOpen) { return; }

    Batch batch;
    batch.Kind = _openKind;
    batch.Texture = _openTexture;
    batch.Indices = std::move(_openIndices);

    std::size_t vertexCount = 0;
    switch (_openKind) {
        case BatchKind::Rect:
            vertexCount = _openRectVertices.size();
            batch.Vertices = std::move(_openRectVertices);
            break;
        case BatchKind::Image:
            vertexCount = _openImageVertices.size();
            batch.Vertices = std::move(_openImageVertices);
            break;
        case BatchKind::Text:
            vertexCount = _openTextVertices.size();
            batch.Vertices = std::move(_openTextVertices);
            break;
        case BatchKind::Shadow:
        case BatchKind::Layer:
        case BatchKind::BackdropBlur:
            // Unreachable: see OpenVertexCount()'s identical comment.
            break;
    }

    _stats.DrawCallCount += 1;
    _stats.VertexCount += static_cast<std::uint32_t>(vertexCount);
    _stats.FlushReasonCounts[static_cast<std::size_t>(reason)] += 1;

    _batches.push_back(std::move(batch));

    _hasOpen = false;
    _openRectVertices.clear();
    _openImageVertices.clear();
    _openTextVertices.clear();
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

void BatchBuilder::AppendText(const Rendering::DrawText& cmd) {
    if (_textLayoutProvider == nullptr) {
        FlushOpen(BatchFlushReason::NonBatchable);
        return;
    }

    std::vector<ITextLayoutProvider::GlyphQuad> quads;
    const Rendering::TextureId texture = _textLayoutProvider->LayoutText(cmd, quads);
    if (!texture.IsValid() || quads.empty()) {
        FlushOpen(BatchFlushReason::NonBatchable);
        return;
    }

    // Per-quad, not per-command, so a single very-long DrawText run still force-flushes partway
    // through rather than overshooting kMaxBatchVertices by an unbounded amount (unlike
    // AppendRect/AppendImage's per-command check, safe there only because those always add
    // exactly 4 vertices per command).
    for (const ITextLayoutProvider::GlyphQuad& quad : quads) {
        const bool kindChanged = _hasOpen && _openKind != BatchKind::Text;
        const bool textureChanged = _hasOpen && !(_openTexture == texture);

        if (kindChanged) {
            FlushOpen(BatchFlushReason::CommandTypeChange);
        } else if (textureChanged) {
            FlushOpen(BatchFlushReason::TextureChange);
        } else if (_hasOpen && OpenVertexCount() + 4 > kMaxBatchVertices) {
            FlushOpen(BatchFlushReason::BufferFull);
        }

        if (!_hasOpen) { BeginBatch(BatchKind::Text, texture); }

        const auto base = static_cast<std::uint32_t>(_openTextVertices.size());
        _openTextVertices.push_back(TextVertex{
            .Position = {quad.Min.x, quad.Min.y}, .Uv = {quad.UvMin.x, quad.UvMin.y}, .Color = cmd.Color});
        _openTextVertices.push_back(TextVertex{
            .Position = {quad.Max.x, quad.Min.y}, .Uv = {quad.UvMax.x, quad.UvMin.y}, .Color = cmd.Color});
        _openTextVertices.push_back(TextVertex{
            .Position = {quad.Max.x, quad.Max.y}, .Uv = {quad.UvMax.x, quad.UvMax.y}, .Color = cmd.Color});
        _openTextVertices.push_back(TextVertex{
            .Position = {quad.Min.x, quad.Max.y}, .Uv = {quad.UvMin.x, quad.UvMax.y}, .Color = cmd.Color});

        _openIndices.push_back(base + 0);
        _openIndices.push_back(base + 1);
        _openIndices.push_back(base + 2);
        _openIndices.push_back(base + 0);
        _openIndices.push_back(base + 2);
        _openIndices.push_back(base + 3);
    }
}

void BatchBuilder::AppendShadow(const Rendering::DrawShadow& cmd) {
    // A Shadow batch never accumulates a second item (see this class's own header comment), so
    // there's no "is a Shadow batch already open" check the way Append{Rect,Image,Text} have —
    // whatever kind of batch WAS open (if any) closes first, preserving draw order, then this
    // shadow's own single-item batch is built and appended directly, already closed.
    FlushOpen(BatchFlushReason::CommandTypeChange);

    Batch batch;
    batch.Kind = BatchKind::Shadow;
    batch.Vertices = std::vector<ShadowVertex>{ShadowVertex{
        .Position = cmd.Position,
        .Size = cmd.Size,
        .Radii = cmd.Radii,
        .BlurRadius = cmd.BlurRadius,
        .Offset = cmd.Offset,
        .ShadowColor = cmd.ShadowColor,
        .Spread = cmd.Spread,
    }};

    _stats.DrawCallCount += 1;
    _stats.VertexCount += 1; // one ShadowVertex record, not a literal GPU vertex -- kept for stats consistency.
    _stats.FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::CommandTypeChange)] += 1;

    _batches.push_back(std::move(batch));
}

void BatchBuilder::AppendLayerMarker(LayerVertex marker) {
    // Same single-item, always-closed shape as AppendShadow() -- see that method's own comment.
    FlushOpen(BatchFlushReason::LayerChange);

    Batch batch;
    batch.Kind = BatchKind::Layer;
    batch.Vertices = std::vector<LayerVertex>{marker};

    _stats.DrawCallCount += 1;
    _stats.VertexCount += 1; // one LayerVertex record, not a literal GPU vertex -- see AppendShadow()'s identical note.
    _stats.FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::LayerChange)] += 1;

    _batches.push_back(std::move(batch));
}

void BatchBuilder::AppendBackdropBlur(const Rendering::DrawBackdropBlur& cmd) {
    // Same single-item, always-closed shape as AppendShadow() -- see that method's own comment.
    FlushOpen(BatchFlushReason::CommandTypeChange);

    Batch batch;
    batch.Kind = BatchKind::BackdropBlur;
    batch.Vertices = std::vector<BackdropBlurVertex>{BackdropBlurVertex{
        .Position = cmd.Position,
        .Size = cmd.Size,
        .Radii = cmd.Radii,
        .BlurRadius = cmd.BlurRadius,
        .TintColor = cmd.TintColor,
    }};

    _stats.DrawCallCount += 1;
    _stats.VertexCount += 1; // one BackdropBlurVertex record, not a literal GPU vertex -- see AppendShadow()'s note.
    _stats.FlushReasonCounts[static_cast<std::size_t>(BatchFlushReason::CommandTypeChange)] += 1;

    _batches.push_back(std::move(batch));
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
                } else if constexpr (std::is_same_v<T, Rendering::DrawText>) {
                    AppendText(cmd);
                } else if constexpr (std::is_same_v<T, Rendering::DrawShadow>) {
                    AppendShadow(cmd);
                } else if constexpr (std::is_same_v<T, Rendering::DrawPath>) {
                    FlushOpen(BatchFlushReason::NonBatchable);
                } else if constexpr (std::is_same_v<T, Rendering::PushClipRect> ||
                                      std::is_same_v<T, Rendering::PopClipRect>) {
                    FlushOpen(BatchFlushReason::ClipRectChange);
                } else if constexpr (std::is_same_v<T, Rendering::PushOpacityLayer>) {
                    AppendLayerMarker(LayerVertex{.Op = LayerOp::PushOpacity, .Opacity = cmd.Opacity});
                } else if constexpr (std::is_same_v<T, Rendering::PushBlendLayer>) {
                    AppendLayerMarker(LayerVertex{.Op = LayerOp::PushBlend, .Mode = cmd.Mode});
                } else if constexpr (std::is_same_v<T, Rendering::PopLayer>) {
                    AppendLayerMarker(LayerVertex{.Op = LayerOp::Pop});
                } else if constexpr (std::is_same_v<T, Rendering::DrawBackdropBlur>) {
                    AppendBackdropBlur(cmd);
                }
            },
            command);
    }

    FlushOpen(BatchFlushReason::EndOfBuffer);
}

} // namespace ImFrame::Internal
