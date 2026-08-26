/**
 * @file     BatchBuilder.hpp
 * @brief    Groups adjacent `CommandBuffer` commands into GPU-ready vertex batches
 *
 * @internal
 * `BatchBuilder::Build()` walks a finished `Rendering::CommandBuffer` once and
 * accumulates `DrawRect`/`DrawImage`/`DrawText` geometry into `Batch` records —
 * each `Batch` is exactly one draw call for a real GPU backend. A batch is
 * flushed (closed, appended to `Batches()`) whenever the command type changes,
 * the texture changes, the active clip/layer state changes, a non-batchable
 * command is encountered, or the batch's vertex count reaches `kMaxBatchVertices`.
 *
 * `RectVertex`/`ImageVertex`/`TextVertex` are deliberately kind-specific, not
 * one generic `Vertex` struct — they match `Shaders/Rect.glsl`/`Shaders/
 * Image.glsl`/`Shaders/MSDFText.glsl`'s `STAGE_VERTEX` inputs field-for-field
 * (Phase 32.2's rounded-box SDF shaders need per-rect `Local`/`HalfSize`/
 * `Radii`, which a generic Position/Uv/Color vertex cannot carry). `DrawPath`
 * remains non-batchable here — `Shaders/Path.glsl` exists, but nothing in the
 * tree emits `DrawPath` yet (see `.claude/DECISIONS.md`, Phase 31.2) and the
 * CPU polyline tesselator it would need is separate, deferred work; batching
 * it now would be speculative. See `.claude/DECISIONS.md`, Phase 32.3.
 * `DrawShadow` also remains non-batchable — no live producer, same reasoning.
 *
 * `DrawText` is batchable only when a `ITextLayoutProvider` has been set via
 * `SetTextLayoutProvider()` *and* it resolves `cmd.Font` to a valid glyph-atlas
 * texture — otherwise it falls back to the same non-batchable skip `DrawPath`/
 * `DrawShadow` get. See `ITextLayoutProvider`'s own doc comment and
 * `.claude/DECISIONS.md`, Phase 33.6, for why this indirection exists (it lets
 * `BatchBuilder` stay free of any `FontRegistry`/`TextShaper`/`GlyphAtlas`
 * dependency).
 *
 * This is a pure CPU-side data structure — it has no GPU calls and no
 * dependency on any `IRenderer` implementation, so it is unit-testable in
 * isolation (`ITextLayoutProvider` is an abstract seam, not a concrete
 * dependency — see its own doc comment).
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

/// Vertex layout for `BatchKind::Text` — matches `Shaders/MSDFText.glsl`'s `STAGE_VERTEX` inputs exactly.
struct TextVertex {
    Widgets::Vec2 Position{};                    ///< Pixel-space vertex position (`inPosition`).
    Widgets::Vec2 Uv{0.0f, 0.0f};                 ///< MSDF glyph-atlas texture coordinate (`inUv`).
    Widgets::Vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};  ///< Per-vertex glyph colour (`inTextColor`).
};

/**
 * @class    ITextLayoutProvider
 * @brief    Turns one `Rendering::DrawText` command into positioned, atlas-backed glyph quads
 *
 * @internal
 * `BatchBuilder` itself has zero dependency on `FontRegistry`/`TextShaper`/`GlyphAtlas` (Phase 33's
 * text pipeline, gated behind the separate `IMF_BUILD_TEXT_MSDF` option, independent of
 * `IMF_BUILD_NATIVE_RENDERER`) — this interface is the seam that keeps it that way, so
 * `BatchBuilder` stays buildable and unit-testable under `IMF_BUILD_NATIVE_RENDERER` alone,
 * exactly as it was before Phase 33.6. A concrete implementation adapting the real
 * `FontRegistry`+`TextShaper`+`GlyphAtlas` pipeline is `NativeRendererGL3`'s job (a later
 * sub-phase), which already needs both options enabled to render MSDF text at all. See
 * `.claude/DECISIONS.md`, Phase 33.6.
 *
 * @since    3.0.0
 */
class ITextLayoutProvider {
public:
    virtual ~ITextLayoutProvider() noexcept = default;

    /// One positioned glyph quad ready for `TextVertex` emission, in the same pixel-space
    /// `Rendering::DrawText::Position` is expressed in.
    struct GlyphQuad {
        Widgets::Vec2 Min{}; ///< Top-left corner, pixels.
        Widgets::Vec2 Max{}; ///< Bottom-right corner, pixels.
        Widgets::Vec2 UvMin{};
        Widgets::Vec2 UvMax{};
    };

    /**
     * @brief    Shapes and lays out `cmd`, appending one `GlyphQuad` per visible glyph to `outQuads`.
     *
     * @param[in]  cmd        The command to lay out (`Text`/`Font`/`FontSize`/`Position`).
     * @param[out] outQuads   Receives one entry per non-blank glyph, in shaping order. Not cleared first.
     * @return   The glyph-atlas texture every appended quad's UV is relative to, or a default
     *           (invalid) `Rendering::TextureId` if `cmd.Font` is unknown/unresolvable —
     *           `BatchBuilder` then skips `cmd` entirely (`BatchFlushReason::NonBatchable`),
     *           matching `FontRegistry::Get()`/`GlyphAtlas::GetOrCreate()`'s own "invalid id in,
     *           null/nothing out" convention.
     * @throws   Nothing.
     *
     * `Rendering::DrawText::MaxWidth` wrapping is not implemented by any provider as of Phase
     * 33.6 — every `DrawText` lays out as a single line regardless of `MaxWidth`.
     */
    [[nodiscard]] virtual Rendering::TextureId LayoutText(const Rendering::DrawText& cmd,
                                                           std::vector<GlyphQuad>& outQuads) = 0;
};

/// What kind of geometry a `Batch` holds — batches never mix kinds, matching the "command type change" flush trigger.
enum class BatchKind : unsigned char {
    Rect,  ///< From `Rendering::DrawRect` — solid-colour geometry, no texture. Vertices are `RectVertex`.
    Image, ///< From `Rendering::DrawImage` — textured geometry. Vertices are `ImageVertex`.
    Text,  ///< From `Rendering::DrawText`, when a `ITextLayoutProvider` is set. Vertices are `TextVertex`.
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

/// Kind-specific vertex storage for one `Batch` — holds `RectVertex`/`ImageVertex`/`TextVertex` depending on `Batch::Kind`.
using BatchVertices = std::variant<std::vector<RectVertex>, std::vector<ImageVertex>, std::vector<TextVertex>>;

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

    /**
     * @brief    Sets the collaborator `DrawText` commands are laid out through.
     * @param[in] provider  Non-owning; must outlive this `BatchBuilder` or be cleared (`nullptr`)
     *                      before it is destroyed. `nullptr` (the default) makes every `DrawText`
     *                      skip as `BatchFlushReason::NonBatchable`, matching this class's
     *                      behaviour before Phase 33.6.
     */
    void SetTextLayoutProvider(ITextLayoutProvider* provider) noexcept { _textLayoutProvider = provider; }

    [[nodiscard]] const std::vector<Batch>& Batches() const noexcept { return _batches; }
    [[nodiscard]] const BatchStats&         Stats() const noexcept { return _stats; }

private:
    void FlushOpen(BatchFlushReason reason);
    void BeginBatch(BatchKind kind, Rendering::TextureId texture);
    void AppendRect(const Rendering::DrawRect& cmd);
    void AppendImage(const Rendering::DrawImage& cmd);
    void AppendText(const Rendering::DrawText& cmd);
    [[nodiscard]] std::size_t OpenVertexCount() const noexcept;

    std::vector<Batch> _batches;
    BatchStats          _stats;

    bool                  _hasOpen = false;
    BatchKind             _openKind{BatchKind::Rect};
    Rendering::TextureId  _openTexture{};
    std::vector<RectVertex>    _openRectVertices;
    std::vector<ImageVertex>   _openImageVertices;
    std::vector<TextVertex>    _openTextVertices;
    std::vector<std::uint32_t> _openIndices;

    ITextLayoutProvider* _textLayoutProvider = nullptr;
};

} // namespace ImFrame::Internal
