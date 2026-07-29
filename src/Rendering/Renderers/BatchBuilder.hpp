/**
 * @file     BatchBuilder.hpp
 * @brief    Groups adjacent `CommandBuffer` commands into GPU-ready vertex batches
 *
 * @internal
 * `BatchBuilder::Build()` walks a finished `Rendering::CommandBuffer` once and
 * accumulates `DrawRect`/`DrawImage` geometry into `Batch` records — each
 * `Batch` is exactly one draw call for a real GPU backend. A batch is flushed
 * (closed, appended to `Batches()`) whenever the command type changes, the
 * texture changes, the active clip/layer state changes, a non-batchable
 * command is encountered (`DrawPath`/`DrawText`/`DrawShadow` in Phase 32 —
 * see `PHASE_32_PROPOSAL.md`'s Batching section), or the batch's vertex count
 * reaches `kMaxBatchVertices`.
 *
 * `RectVertex`/`ImageVertex` are deliberately kind-specific, not one generic
 * `Vertex` struct — they match `Shaders/Rect.glsl`/`Shaders/Image.glsl`'s
 * `STAGE_VERTEX` inputs field-for-field (Phase 32.2's rounded-box SDF shaders
 * need per-rect `Local`/`HalfSize`/`Radii`, which a generic
 * Position/Uv/Color vertex cannot carry). `DrawPath` remains non-batchable
 * here — `Shaders/Path.glsl` exists, but nothing in the tree emits `DrawPath`
 * yet (see `.claude/DECISIONS.md`, Phase 31.2) and the CPU polyline
 * tesselator it would need is separate, deferred work; batching it now would
 * be speculative. See `.claude/DECISIONS.md`, Phase 32.3.
 *
 * This is a pure CPU-side data structure — it has no GPU calls and no
 * dependency on any `IRenderer` implementation, so it is unit-testable in
 * isolation.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Rendering/CommandBuffer.hpp"

#include <array>
#include <cstdint>
#include <variant>
#include <vector>

namespace ImFrame::Internal {

/// Vertex layout for `BatchKind::Rect` — matches `Shaders/Rect.glsl`'s `STAGE_VERTEX` inputs exactly.
struct RectVertex {
    Widgets::Vec2           Position{};    ///< Pixel-space vertex position (`inPosition`).
    Widgets::Vec2           Local{};       ///< Position relative to the rect's center, in pixels (`inLocal`).
    Widgets::Vec2           HalfSize{};    ///< Rect half-width/half-height, in pixels (`inHalfSize`).
    Rendering::CornerRadii  Radii{};       ///< Per-corner radii, same field order as `DrawRect::Radii` (`inRadii`).
    Widgets::Vec4           FillColor{0.0f, 0.0f, 0.0f, 0.0f};
    Widgets::Vec4           StrokeColor{0.0f, 0.0f, 0.0f, 0.0f};
    float                   StrokeWidth = 0.0f;
};

/// Vertex layout for `BatchKind::Image` — matches `Shaders/Image.glsl`'s `STAGE_VERTEX` inputs exactly.
struct ImageVertex {
    Widgets::Vec2           Position{};
    Widgets::Vec2           Local{};
    Widgets::Vec2           HalfSize{};
    Rendering::CornerRadii  Radii{};
    Widgets::Vec2           Uv{0.0f, 0.0f};
    Widgets::Vec4           TintColor{1.0f, 1.0f, 1.0f, 1.0f};
};

/// What kind of geometry a `Batch` holds — batches never mix kinds, matching the "command type change" flush trigger.
enum class BatchKind : unsigned char {
    Rect,  ///< From `Rendering::DrawRect` — solid-colour geometry, no texture. Vertices are `RectVertex`.
    Image, ///< From `Rendering::DrawImage` — textured geometry. Vertices are `ImageVertex`.
};

/// Why a batch was closed. Backs `BatchStats::FlushReasonCounts`.
enum class BatchFlushReason : unsigned char {
    CommandTypeChange, ///< `BatchKind` changed (e.g. `DrawRect` run followed by a `DrawImage`).
    TextureChange,     ///< Same `BatchKind` but a different `Rendering::TextureId`.
    ClipRectChange,    ///< `PushClipRect`/`PopClipRect` encountered.
    LayerChange,       ///< `PushOpacityLayer`/`PushBlendLayer`/`PopLayer` encountered.
    NonBatchable,      ///< `DrawPath`/`DrawText`/`DrawShadow` encountered — Phase 32 does not batch these.
    BufferFull,        ///< The open batch reached `kMaxBatchVertices`.
    EndOfBuffer,       ///< The command buffer ended with a batch still open.
};

/// Number of distinct `BatchFlushReason` values — sizes `BatchStats::FlushReasonCounts`.
inline constexpr std::size_t kBatchFlushReasonCount = 7;

/// Kind-specific vertex storage for one `Batch` — holds `RectVertex` or `ImageVertex` depending on `Batch::Kind`.
using BatchVertices = std::variant<std::vector<RectVertex>, std::vector<ImageVertex>>;

/// One closed batch: exactly one draw call's worth of geometry, sharing a texture and clip/layer state.
struct Batch {
    BatchKind                  Kind{BatchKind::Rect};
    Rendering::TextureId       Texture{}; ///< Default (invalid) `TextureId` for `BatchKind::Rect` — no texture.
    BatchVertices               Vertices;
    std::vector<std::uint32_t> Indices;
};

/// Per-frame batching diagnostics, exposed by a future `NativeRenderer::Stats()` for `PerfOverlay`.
struct BatchStats {
    std::uint32_t DrawCallCount = 0; ///< Number of closed batches — one draw call each.
    std::uint32_t VertexCount   = 0; ///< Total vertices across all closed batches.
    std::array<std::uint32_t, kBatchFlushReasonCount> FlushReasonCounts{};
};

/**
 * @class    BatchBuilder
 * @brief    Turns a recorded `Rendering::CommandBuffer` into a sequence of GPU-ready `Batch` records
 *
 * @since    3.0.0
 *
 * @example
 * @code
 * BatchBuilder builder;
 * builder.Build(commandBuffer);
 * for (const Batch& batch : builder.Batches()) {
 *     // upload batch.Vertices/batch.Indices (kind-specific — std::get<vector<RectVertex>>(batch.Vertices)
 *     // when batch.Kind == BatchKind::Rect) and issue one draw call with the matching shader pipeline
 * }
 * @endcode
 */
class BatchBuilder {
public:
    /// Maximum vertices held by one open batch before it is force-flushed with `BatchFlushReason::BufferFull`.
    static constexpr std::uint32_t kMaxBatchVertices = 4096;

    /// Clears all closed batches and stats, retaining allocated capacity.
    void Reset();

    /**
     * @brief    Walks every command in `buffer`, producing closed batches in `Batches()`.
     * @param[in] buffer  A fully-recorded command buffer (e.g. from `Reconciler::Show()`).
     *
     * Calls `Reset()` first, so each call produces a fresh result independent of prior calls.
     */
    void Build(const Rendering::CommandBuffer& buffer);

    [[nodiscard]] const std::vector<Batch>& Batches() const noexcept { return _batches; }
    [[nodiscard]] const BatchStats&         Stats() const noexcept { return _stats; }

private:
    void FlushOpen(BatchFlushReason reason);
    void BeginBatch(BatchKind kind, Rendering::TextureId texture);
    void AppendRect(const Rendering::DrawRect& cmd);
    void AppendImage(const Rendering::DrawImage& cmd);
    [[nodiscard]] std::size_t OpenVertexCount() const noexcept;

    std::vector<Batch> _batches;
    BatchStats          _stats;

    bool                  _hasOpen = false;
    BatchKind             _openKind{BatchKind::Rect};
    Rendering::TextureId  _openTexture{};
    std::vector<RectVertex>    _openRectVertices;
    std::vector<ImageVertex>   _openImageVertices;
    std::vector<std::uint32_t> _openIndices;
};

} // namespace ImFrame::Internal
