/**
 * @file     NativeRendererWebGPU.hpp
 * @brief    `IRenderer` implementation that renders `Rendering::CommandBuffer` via real WebGPU draw calls
 *
 * @internal
 * Phase 35.8 — WebGPU's own "first SDF rect" sub-phase, mirroring `NativeRendererVulkan`'s (Phase
 * 35.1) and `NativeRendererDX12`'s (Phase 35.7) identical starting scope: `BatchKind::Rect` only,
 * via a real render pipeline, no batching-kind interleaving, no text, no effects yet. Reuses the
 * *same*, backend-agnostic `src/Rendering/Renderers/BatchBuilder.hpp`/`RectVertex` every other
 * `NativeRenderer*` already does — only the WebGPU-specific plumbing (bind group, pipeline,
 * command encoder) is new here.
 *
 * Phase 35.10 adds `BatchKind::Image`, mirroring `NativeRendererVulkan`'s own
 * Phase 35.2: `Rendering::TextureId::Value()` is reinterpreted as a raw `WGPUTextureView`, the
 * WebGPU analogue of Vulkan's own raw-`VkImageView` convention. Unlike Vulkan's descriptor *sets*
 * or DX12's descriptor *heap slots* (both mutable GPU-visible state that must be written before
 * command recording, since their content resolves at execution time, not recording time), a
 * WebGPU `WGPUBindGroup` is **immutable once created** — there is no equivalent race to guard
 * against, so `RenderImageBatch()` simply creates a fresh bind group per `Image` batch inline,
 * uses it, and releases it, with no descriptor-pool/heap capacity bookkeeping needed at all (the
 * one genuine simplification WebGPU's own object model offers over Vulkan/DX12 for this feature).
 * Texture upload itself is also simpler on this backend — `wgpuQueueWriteTexture()` uploads
 * directly with no manual staging-buffer/copy-command/layout-transition dance.
 *
 * Unlike Vulkan (SPIR-V) and DX12 (DXIL, offline-compiled via `cmake/CompileShaderDXC.cmake`),
 * WGSL is compiled at *runtime* by Dawn — no offline compile/embed pipeline is needed. The vertex
 * and fragment shader source is inlined directly as a C++ raw string literal in
 * `NativeRendererWebGPU.cpp`, mirroring `NativeRendererGL3`'s own hand-written-inline-shader
 * convention rather than the `Shaders/*.glsl`/`*.hlsl` file-based CMake pipelines.
 *
 * The inline WGSL vertex shader negates Y, matching `SDFRect.hlsl`'s own (D3D) convention rather
 * than `SDFRect.glsl`'s Vulkan copy — WebGPU's NDC is Y-up (same convention as D3D/Metal), unlike
 * Vulkan's Y-down NDC. This sub-phase's own deliberately off-center asymmetric test (see
 * `Tests/Rendering/NativeRendererWebGPU_test.cpp`) empirically confirms this derivation, matching
 * `NativeRendererDX12_test.cpp`'s identical Phase 35.7 precedent. See `.claude/DECISIONS.md`,
 * Phase 35.8, for the full derivation.
 *
 * Like `NativeRendererVulkan`/`NativeRendererDX12`, this class is constructed with explicit WebGPU
 * handles (not routed through any public, application-facing context type) and needs an explicit
 * `SetTarget()` call before its first `Render()`. Unlike Vulkan/DX12, `Render()` needs no fence or
 * CPU stall at all — `ViewportWebGPU.cpp`'s own comment already documents why: "WebGPU's sequential
 * submit model guarantees the above work completes before the next submit on this queue — no
 * explicit fence needed." A fresh `WGPUCommandEncoder` is created, recorded into, finished, and
 * submitted every `Render()` call, mirroring `ViewportFramebufferWebGPU::BeginRender()`/
 * `EndRender()`'s own identical per-frame pattern.
 *
 * Phase 35.16 adds `BatchKind::Shadow`, wiring `BlurPassWebGPU` (Phase 35.15) into a silhouette-
 * render-then-blur-then-composite flow mirroring `NativeRendererVulkan::RenderShadowBatch()`'s own
 * Phase 35.4 structure — but **needing no resource-state transitions of any kind**, unlike Vulkan's
 * zero-transition-but-still-present `VK_IMAGE_LAYOUT_GENERAL` trick or DX12's real
 * `D3D12_RESOURCE_BARRIER` round-trips (Phase 35.12): WebGPU exposes no explicit resource-state
 * model to the API surface at all (`BlurPassWebGPU.hpp`'s own finding, Phase 35.15), so the
 * silhouette texture is simply created with both `WGPUTextureUsage_RenderAttachment` and
 * `WGPUTextureUsage_StorageBinding` and used directly in either role with no transition of any kind
 * — Dawn's own implementation inserts whatever synchronization is needed automatically. What *does*
 * need real restructuring is `Render()`'s own command-encoder/render-pass threading: unlike Vulkan's
 * `VkCommandBuffer cmd` reference-parameter idiom or DX12's persistent `_commandList` member,
 * `NativeRendererWebGPU` creates a **fresh** `WGPUCommandEncoder`/`WGPURenderPassEncoder` pair per
 * `Render()` call (Phase 35.8's own established convention) — `BeginMainPass()`/
 * `EndAndSubmitMainPass()` promote that pair to `_mainEncoder`/`_mainPass` members so
 * `RenderShadowBatch()` can end/submit the accumulated-so-far pass, do its own separate silhouette-
 * render and `_blurPass.Apply()` work (each its own self-contained encoder/submit), then begin a
 * fresh main pass to resume compositing whatever batches follow — needing **no fence wait** at any
 * of these split points, since WebGPU's own sequential-submit-ordering guarantee (this class's own
 * "no CPU stall" finding above) already ensures each submit's GPU work completes before the next one
 * on this queue, unlike Vulkan's/DX12's own `Begin`/`EndAndSubmit` splits, which still need real
 * fence waits at each step. `RenderRectBatch()` now (re)binds its own pipeline/bind-group
 * unconditionally (matching `RenderImageBatch()`'s own existing convention, and
 * `NativeRendererDX12::RenderRectBatch()`'s identical Phase 35.9 rationale) rather than relying on a
 * single bind before `Render()`'s own loop, since a freshly-begun main pass (after a `Shadow` batch)
 * needs its own state rebound from scratch.
 *
 * Phase 35.17 adds `BatchKind::Layer` (`PushOpacityLayer`/`PushBlendLayer`/`PopLayer`), mirroring
 * `NativeRendererVulkan`'s/`NativeRendererDX12`'s own Phase 35.5/35.13 structure — again needing
 * **no resource-state transitions**: a `LayerTarget`'s texture combines `RenderAttachment` and
 * `TextureBinding` usage and is used directly in either role. `SetTarget()`'s own signature grew a
 * new leading `WGPUTexture targetTexture` parameter (alongside the existing `targetView`), matching
 * `NativeRendererVulkan`'s/`NativeRendererDX12`'s own two-handle `SetTarget()` — `PushBlendLayer`'s
 * own backdrop copy (`wgpuCommandEncoderCopyTextureToTexture()`) needs the real target's own
 * `WGPUTexture`, which no WebGPU API call can recover from a `WGPUTextureView` alone, unlike every
 * other operation this class needed through Phase 35.16 (all of which bind by view only). Unlike
 * Vulkan's/DX12's own "no separate submission needed for Layer" finding, this backend's own
 * composite draws (`CompositeOpacityLayer()`/`CompositeBlendLayer()`) *do* each need their own
 * dedicated `EndAndSubmitMainPass()`/`BeginMainPass()` bracket — not because of any resource-state
 * concern, but because WebGPU has no push-constant equivalent (`Rendering::BlendMode`'s own `Mode`
 * value, and each composite's own quad vertex/index data, all go through ordinary buffers whose
 * *content* resolves at execution time, not recording time). Two *sibling* (non-nested) layer pops
 * within one `Render()` call would otherwise both write the *same* shared `_shadowQuadVertexBuffer`/
 * `_shadowQuadIndexBuffer` before either draw actually executes, corrupting the earlier one once the
 * (shared, not-yet-submitted) command buffer finally runs — and a `Shadow` batch's own composite
 * draw (left unsubmitted in the resumed main pass until the next split) is exposed to the identical
 * overwrite if a layer pop follows it. Submitting everything accumulated *before* each composite's
 * own buffer writes, and the composite draw itself *after*, sidesteps this entirely — the same
 * "content resolves at execution time" hazard
 * `BlurPassWebGPU`'s own two-uniform-buffer finding (Phase 35.15) already established, generalized
 * here to an *unbounded* number of composite draws per `Render()` call rather than a fixed two.
 *
 * Phase 35.18 adds `BatchKind::BackdropBlur`, mirroring `NativeRendererVulkan::RenderBackdropBlurBatch()`'s
 * (Phase 35.6) and `NativeRendererDX12::RenderBackdropBlurBatch()`'s (Phase 35.14) structure: copy the
 * padded requested region out of the current target (`wgpuCommandEncoderCopyTextureToTexture()` with
 * a source origin — no coordinate flip, WebGPU addresses texture memory top-down like Vulkan/DX12),
 * blur it via `_blurPass`, composite back a UV-cropped sub-rectangle at exactly the requested rect
 * with the standard (non-premultiplied) `_imagePipeline`. `_backdropBlurCopyTexture` combines
 * `StorageBinding` (`_blurPass`'s source) and `CopyDst` with no transitions. It copies from
 * `_currentTargetTexture` and composites into `_currentTargetView`, so a backdrop blur inside a
 * pushed layer blurs that layer's own content (Vulkan/DX12 always copy from the real target). Like
 * every other writer of the shared quad buffers, it submits everything accumulated before writing
 * them — the invariant that makes all of these internal composites safe in one `Render()` call.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-14
 * @version  3.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "BlurPassWebGPU.hpp"
#include "Rendering/Renderers/BatchBuilder.hpp"
#include "Rendering/Renderers/IRenderer.hpp"

#include <webgpu/webgpu.h>

#include <cstdint>
#include <vector>

namespace ImFrame::Internal {

/**
 * @class    NativeRendererWebGPU
 * @brief    Renders `Rendering::DrawRect` batches via a real WebGPU render pipeline
 *
 * @internal
 * WebGPU resources (shader module, bind group layout, pipeline layout, render pipeline, uniform
 * buffer, bind group, streaming vertex/index buffers) are created lazily on the first `Render()`
 * call, matching `NativeRendererVulkan::EnsureInitialized()`/`NativeRendererDX12::EnsureInitialized()`'s
 * own lazy-initialization convention — constructing an instance never issues a single WebGPU call.
 *
 * @since    3.1.0
 */
class NativeRendererWebGPU final : public IRenderer {
public:
    /**
     * @brief    Constructs a renderer bound to the given (not owned) WebGPU device/queue.
     * @param[in] device      A valid, already-created `WGPUDevice`.
     * @param[in] queue       This device's own `WGPUQueue` — WebGPU has no explicit queue-family
     *                        selection, unlike Vulkan/DX12, so there is only ever one.
     * @param[in] colorFormat `WGPUTextureFormat` of the target view(s) passed to `SetTarget()` —
     *                        fixed at construction time, matching this sub-phase's scope (one
     *                        target format for this renderer's whole lifetime).
     *
     * Neither handle is owned — the caller (a real backend, or a test) is responsible for their
     * lifetime outliving this object and for calling `Shutdown()` before destroying them.
     */
    NativeRendererWebGPU(WGPUDevice device, WGPUQueue queue, WGPUTextureFormat colorFormat);
    ~NativeRendererWebGPU() override;

    NativeRendererWebGPU(const NativeRendererWebGPU&)            = delete;
    NativeRendererWebGPU& operator=(const NativeRendererWebGPU&) = delete;

    /**
     * @brief    Sets the offscreen target `Render()` draws into.
     * @param[in] targetTexture  The real `WGPUTexture` backing `targetView` (Phase 35.17) — needed
     *                           only by `PushBlendLayer`'s own backdrop copy
     *                           (`wgpuCommandEncoderCopyTextureToTexture()`, which takes no view),
     *                           not by any operation through Phase 35.16. Must include
     *                           `WGPUTextureUsage_CopySrc` if any top-level `PushBlendLayer` is
     *                           ever rendered.
     * @param[in] targetView     A `WGPUTextureView` of format `colorFormat` (the constructor's own
     *                           parameter) — the render-pass color attachment. Neither handle is
     *                           owned; the caller keeps both alive at least until the next
     *                           `SetTarget()`/`Shutdown()` call.
     * @param[in] width          Target width, in pixels.
     * @param[in] height         Target height, in pixels.
     *
     * Must be called at least once before the first `Render()` call.
     */
    void SetTarget(WGPUTexture targetTexture, WGPUTextureView targetView, std::uint32_t width,
                   std::uint32_t height) noexcept;

    void Render(const Rendering::CommandBuffer& buffer) override;

    /// Caps how many `Rendering::DrawBackdropBlur` commands actually run their (real GPU cost)
    /// copy+blur+composite per `Render()` call — matches `NativeRendererVulkan`'s/
    /// `NativeRendererDX12`'s/`NativeRendererGL3`'s own identical rate-limiting role and default.
    void SetMaxBackdropBlurPerFrame(int maxPerFrame) noexcept { _maxBackdropBlurPerFrame = maxPerFrame; }

    /// Always fails — no text pipeline exists yet (this sub-phase's scope is `BatchKind::Rect` only).
    [[nodiscard]] Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) override;

    /// Releases all WebGPU resources this renderer itself created. Safe to call multiple times,
    /// including before any `Render()` call. Does not touch the constructor's borrowed handles.
    void Shutdown() override;

private:
    /// One entry on the layer stack (Phase 35.17) — mirrors `NativeRendererVulkan::LayerFrame`/
    /// `NativeRendererDX12::LayerFrame` field for field, `ParentLayout`/nothing collapsed away
    /// entirely (no resource-state concept exists to track here at all).
    struct LayerFrame {
        LayerOp              Op      = LayerOp::PushOpacity;
        float                 Opacity = 1.0f;
        Rendering::BlendMode  Mode    = Rendering::BlendMode::Normal;
        WGPUTexture           ParentTexture = nullptr; ///< Needed only by `CopyBackdropForBlend()`.
        WGPUTextureView       ParentView    = nullptr; ///< What to resume rendering into on `PopLayer()`.
        std::size_t           TargetIndex   = 0;        ///< Index into `_layerTargets`.
    };

    /// One depth level's offscreen layer target — mirrors `NativeRendererVulkan::LayerTarget`/
    /// `NativeRendererDX12::LayerTarget`. Combines `WGPUTextureUsage_RenderAttachment` (to render
    /// into) and `WGPUTextureUsage_TextureBinding` (for the eventual composite draw's own sampling)
    /// on the *same* texture, used directly in either role with no transition of any kind.
    struct LayerTarget {
        WGPUTexture   Texture = nullptr;
        WGPUTextureView View  = nullptr;
        std::uint32_t Width  = 0;
        std::uint32_t Height = 0;
    };

    void EnsureInitialized();
    void EnsureVertexIndexCapacity(std::size_t vertexBytes, std::size_t indexBytes);
    void EnsureShadowSilhouetteTarget(std::uint32_t width, std::uint32_t height);
    void EnsureLayerTarget(std::size_t depth, std::uint32_t width, std::uint32_t height);
    void EnsureBackdropTarget(std::uint32_t width, std::uint32_t height);

    /// Creates a fresh `_mainEncoder`/`_mainPass` pair targeting `_currentTargetView` (defaulting to
    /// `WGPULoadOp_Load`, so previously-submitted content is preserved), and sets the viewport/
    /// scissor. Mirrors `NativeRendererDX12::BeginMainCommandList()`'s own role, minus any
    /// descriptor-heap binding (WebGPU has no such concept) and minus any pipeline/bind-group
    /// binding (each `RenderXBatch()` rebinds its own, unconditionally).
    void BeginMainPass(WGPULoadOp loadOp = WGPULoadOp_Load);
    /// Ends `_mainPass`, finishes and submits `_mainEncoder` -- no fence wait needed (see this
    /// class's own file comment on why WebGPU's sequential submit model makes this safe).
    void EndAndSubmitMainPass();
    /// Ends `_mainPass` and begins a *new* one targeting `view`, within the *same*, still-open
    /// `_mainEncoder` — no submission of any kind (unlike `EndAndSubmitMainPass()`/`BeginMainPass()`
    /// used together). Used by `PushLayer()`/`PopLayer()` for the layer-target/parent-target switch
    /// itself, which (unlike either method's own composite draw) writes no shared buffer and so
    /// needs no submission-based isolation (Phase 35.17's own file comment).
    void SwitchMainPassTarget(WGPUTextureView view, WGPULoadOp loadOp);

    void RenderRectBatch(WGPURenderPassEncoder pass, const Batch& batch, std::size_t& vertexByteOffset,
                          std::size_t& indexByteOffset);

    /**
     * @brief    Records one `BatchKind::Image` batch's draw call.
     *
     * Creates a fresh `WGPUBindGroup` referencing `batch.Texture` (reinterpreted as a
     * `WGPUTextureView`), `_linearSampler`, and `_perFrameBuffer` — used immediately for this
     * batch's own `SetBindGroup`/draw, then released before returning. Safe to do per-batch,
     * unlike Vulkan's per-batch descriptor-*set* write or DX12's per-batch descriptor-heap-*slot*
     * write: a `WGPUBindGroup` is immutable from the moment it's created, so there is no "written
     * before recording, else a later batch's write clobbers an earlier one" race to avoid here —
     * see this class's own file comment.
     */
    void RenderImageBatch(WGPURenderPassEncoder pass, const Batch& batch, std::size_t& vertexByteOffset,
                          std::size_t& indexByteOffset);

    /**
     * @brief    Renders one `BatchKind::Shadow` batch: silhouette, blur, composite (Phase 35.16).
     *
     * Ends and submits `_mainPass`/`_mainEncoder` as accumulated so far (`_blurPass.Apply()` is its
     * own separate submission), renders the shape's silhouette into `_shadowSilhouetteView` via its
     * own one-shot encoder/render pass (reusing `_rectPipeline`, `_bindGroup`/`_perFrameBuffer`
     * temporarily rewritten to the silhouette's own small coordinate space), calls
     * `_blurPass.Apply()`, then begins a *new* main pass and records the composite draw (`_imagePipeline`,
     * a fresh bind group referencing the blurred result's own view) before restoring
     * `_perFrameBuffer`'s real-target content for whatever batches follow. No resource-state
     * transition of any kind is needed anywhere in this sequence — see this class's own file
     * comment.
     */
    void RenderShadowBatch(const Batch& batch);

    /// Dispatches one `BatchKind::Layer` marker (Phase 35.17) to `PushLayer()`/`PopLayer()`.
    void HandleLayerMarker(const Batch& batch);
    void PushLayer(LayerOp op, float opacity, Rendering::BlendMode mode);
    void PopLayer();
    void CompositeOpacityLayer(const LayerFrame& frame);
    /// Copies `frame.ParentTexture` into `_backdropTexture` via
    /// `wgpuCommandEncoderCopyTextureToTexture()` on its own one-shot encoder/submit — an
    /// encoder-level command, illegal while a render pass is open (the same restriction Vulkan's
    /// own `vkCmdCopyImage` has inside a rendering instance, unlike DX12, which has none). Called
    /// by `PopLayer()` only after the parent's content has already been submitted.
    void CopyBackdropForBlend(const LayerFrame& frame);
    void CompositeBlendLayer(const LayerFrame& frame);

    void EnsureBackdropBlurCopyTarget(std::uint32_t width, std::uint32_t height);

    /**
     * @brief    Renders one `BatchKind::BackdropBlur` batch: copy, blur, cropped composite (Phase 35.18).
     *
     * Subject to `_maxBackdropBlurPerFrame`'s rate limit, checked and incremented first, matching
     * `NativeRendererVulkan`'s/`NativeRendererDX12`'s own placement. See this class's own file
     * comment for the full sequence.
     */
    void RenderBackdropBlurBatch(const Batch& batch);

    // ─── Borrowed (not owned) ──────────────────────────────────────────────────
    WGPUDevice        _device      = nullptr;
    WGPUQueue         _queue       = nullptr;
    WGPUTextureFormat _colorFormat = WGPUTextureFormat_Undefined;

    // ─── Render target (set via SetTarget()) ───────────────────────────────────
    WGPUTexture     _targetTexture = nullptr;
    WGPUTextureView _targetView   = nullptr;
    std::uint32_t   _targetWidth  = 0;
    std::uint32_t   _targetHeight = 0;

    // ─── Lazily-created, owned resources ────────────────────────────────────────
    bool _initialized = false;

    WGPUShaderModule    _shaderModule    = nullptr;
    WGPUBindGroupLayout _bindGroupLayout = nullptr;
    WGPUPipelineLayout  _pipelineLayout  = nullptr;
    WGPURenderPipeline  _rectPipeline    = nullptr;

    /// `Shaders::kImageWgsl`'s own shader module, bind group layout, pipeline layout, and pipeline
    /// — kept separate from the Rect pipeline's own equivalents, mirroring
    /// `NativeRendererVulkan`'s/`NativeRendererDX12`'s own separate Image pipelines (Phase
    /// 35.2/35.9). `_linearSampler` is the one shared sampler every `Image` batch's bind group
    /// references — the WebGPU analogue of Vulkan's single shared `VkSampler`/DX12's one static
    /// sampler.
    WGPUShaderModule    _imageShaderModule    = nullptr;
    WGPUBindGroupLayout _imageBindGroupLayout = nullptr;
    WGPUPipelineLayout  _imagePipelineLayout  = nullptr;
    WGPURenderPipeline  _imagePipeline        = nullptr;
    WGPUSampler         _linearSampler        = nullptr;

    /// The inline WGSL's `uPerFrame` uniform (viewport size), updated once per `Render()` call via
    /// `wgpuQueueWriteBuffer()` — WebGPU has no root-CBV/push-constant equivalent in this sub-phase's
    /// scope, so a real bind group is needed even for this one small value.
    WGPUBuffer    _perFrameBuffer = nullptr;
    WGPUBindGroup _bindGroup      = nullptr;

    /// Streaming vertex/index buffers, re-uploaded per `Render()` call via `wgpuQueueWriteBuffer()`
    /// (WebGPU buffers are not persistently mapped the way Vulkan/DX12's `HOST_VISIBLE`/`UPLOAD`
    /// buffers are in this codebase's other native renderers) — grown (never shrunk) on demand.
    WGPUBuffer  _vertexBuffer                 = nullptr;
    std::size_t _vertexBufferCapacityBytes    = 0;
    WGPUBuffer  _indexBuffer                  = nullptr;
    std::size_t _indexBufferCapacityBytes     = 0;

    /// The "currently accumulating" main command encoder/render pass — promoted from `Render()`'s
    /// own former local variables so `RenderShadowBatch()` can end/resume them mid-call. Non-null
    /// only between a `BeginMainPass()`/`EndAndSubmitMainPass()` pair.
    WGPUCommandEncoder    _mainEncoder = nullptr;
    WGPURenderPassEncoder _mainPass    = nullptr;

    /// Which texture/view `_mainPass` currently renders into — the real target, or the innermost
    /// pushed layer's own target (Phase 35.17). Reset to `_targetTexture`/`_targetView` at the start
    /// of every `Render()` call; `BeginMainPass()` always targets `_currentTargetView`.
    WGPUTexture     _currentTargetTexture = nullptr;
    WGPUTextureView _currentTargetView    = nullptr;

    // ─── Phase 35.16: BatchKind::Shadow support ─────────────────────────────────
    BlurPassWebGPU _blurPass;

    /// The offscreen silhouette target — created with both `WGPUTextureUsage_RenderAttachment` (for
    /// the silhouette draw) and `WGPUTextureUsage_StorageBinding` (for `_blurPass`'s own read), used
    /// directly in either role with no transition of any kind (see this class's own file comment on
    /// why, unlike Vulkan's/DX12's own equivalents).
    WGPUTexture     _shadowSilhouetteTexture = nullptr;
    WGPUTextureView _shadowSilhouetteView    = nullptr;
    std::uint32_t   _shadowSilhouetteWidth   = 0;
    std::uint32_t   _shadowSilhouetteHeight  = 0;

    /// Small, fixed-size, dedicated buffers for `RenderShadowBatch()`'s own two one-quad draws
    /// (silhouette rect, composite image) — sized for the *larger* of `RectVertex`/`ImageVertex` (4
    /// vertices), rewritten via `wgpuQueueWriteBuffer()` immediately before each use, mirroring
    /// `NativeRendererVulkan`'s/`NativeRendererDX12`'s own dedicated `_shadowQuadVertexBuffer`/
    /// `_shadowQuadIndexBuffer` (Phase 35.4/35.12).
    WGPUBuffer _shadowQuadVertexBuffer = nullptr;
    WGPUBuffer _shadowQuadIndexBuffer  = nullptr;

    // ─── Phase 35.17: BatchKind::Layer support ──────────────────────────────────
    /// Second Image pipeline sharing `_imagePipelineLayout`/`_imageShaderModule`, only its blend
    /// factors differ (`One`/`OneMinusSrcAlpha`) — for `CompositeOpacityLayer()`'s own
    /// already-premultiplied captured render scaled by `Opacity`, mirroring
    /// `NativeRendererVulkan`'s/`NativeRendererDX12`'s own premultiplied Image pipeline.
    WGPURenderPipeline _premultipliedImagePipeline = nullptr;

    /// The inline `kBlendWgsl` shader's own module, bind group layout (`Mode` uniform b0, source
    /// texture b1, backdrop texture b2, sampler b3 — all fragment-stage, no vertex input), pipeline
    /// layout, and pipeline (blending disabled — the shader computes the full composited result).
    WGPUShaderModule    _blendShaderModule    = nullptr;
    WGPUBindGroupLayout _blendBindGroupLayout = nullptr;
    WGPUPipelineLayout  _blendPipelineLayout  = nullptr;
    WGPURenderPipeline  _blendPipeline        = nullptr;
    /// `Mode` (as an `i32`, padded to 16 bytes) — reused by every blend composite, safe only because
    /// each composite draw is isolated by its own submission (see this class's own file comment).
    WGPUBuffer          _blendModeBuffer      = nullptr;

    std::vector<LayerFrame>  _layerStack;
    std::vector<LayerTarget> _layerTargets;

    /// Blend-mode backdrop copy target — mirrors `NativeRendererVulkan::_backdropImage`/
    /// `NativeRendererDX12::_backdropResource`. `TextureBinding | CopyDst` only.
    WGPUTexture     _backdropTexture = nullptr;
    WGPUTextureView _backdropView    = nullptr;
    std::uint32_t   _backdropWidth   = 0;
    std::uint32_t   _backdropHeight  = 0;

    // ─── Phase 35.18: BatchKind::BackdropBlur support ───────────────────────────
    /// The padded region `RenderBackdropBlurBatch()` is about to blur — kept separate from
    /// `_backdropTexture` (sized to the padded *requested rect*, consumed by `_blurPass` as a
    /// storage texture rather than sampled), mirroring Vulkan's/DX12's own identical separation.
    WGPUTexture     _backdropBlurCopyTexture = nullptr;
    WGPUTextureView _backdropBlurCopyView    = nullptr;
    std::uint32_t   _backdropBlurCopyWidth   = 0;
    std::uint32_t   _backdropBlurCopyHeight  = 0;

    /// See `SetMaxBackdropBlurPerFrame()`. `_backdropBlurCountThisFrame` resets to 0 at the start of
    /// every `Render()` call — "per frame" means "per `Render()` call".
    int _maxBackdropBlurPerFrame    = 4;
    int _backdropBlurCountThisFrame = 0;

    BatchBuilder _batchBuilder;
};

} // namespace ImFrame::Internal
