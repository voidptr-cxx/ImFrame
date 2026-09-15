/**
 * @file     NativeRendererDX12.hpp
 * @brief    `IRenderer` implementation that renders `Rendering::CommandBuffer` via real D3D12 draw calls
 *
 * @internal
 * Phase 35.7 — DX12's own "first SDF rect" sub-phase, mirroring `NativeRendererVulkan`'s identical
 * starting scope (Phase 35.1): `BatchKind::Rect` only, via a real graphics pipeline, no batching-
 * kind interleaving, no text, no effects yet. Reuses the *same*, backend-agnostic
 * `src/Rendering/Renderers/BatchBuilder.hpp`/`RectVertex` every other `NativeRenderer*` already
 * does — only the graphics-API-specific plumbing (root signature, PSO, command list) is new here.
 *
 * Phase 35.9 adds `BatchKind::Image`, mirroring `NativeRendererVulkan`'s own Phase 35.2:
 * `Rendering::TextureId::Value()` is reinterpreted as a raw `ID3D12Resource*`, already in
 * `D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE` at `Render()` time. Unlike Vulkan's combined-
 * image-sampler descriptor (one binding for both view and sampler), D3D12 has no such combined
 * type — `Shaders/Image.hlsl` binds a `Texture2D` SRV (t0, via a descriptor table) and a fixed
 * root-signature *static sampler* (s0, the D3D12 analogue of Vulkan's one shared `VkSampler`).
 * Because a descriptor heap's *content* is resolved by the GPU at command-list *execution* time,
 * not at `SetGraphicsRootDescriptorTable()` *recording* time, `Render()` allocates/writes one SRV
 * per `Image` batch into `_srvHeap` — a shader-visible `CBV_SRV_UAV` heap grown on demand
 * (`EnsureImageDescriptorCapacity()`) — entirely before any command-list recording begins, the
 * exact same reasoning `NativeRendererVulkan::Render()`'s own per-batch descriptor-set allocation
 * already established.
 *
 * Consumes `Shaders/SDFRect.hlsl`/`Shaders/Image.hlsl`'s compiled DXIL directly (Phase 35.7's own
 * `cmake/CompileShaderDXC.cmake` pipeline, embedded as
 * `ImFrame::Internal::Shaders::kSDFRectVertexDxil`/`kSDFRectPixelDxil`/`kImageVertexDxil`/
 * `kImagePixelDxil`). Both shaders' own vertex stage negates Y, matching `NativeRendererGL3`'s own
 * convention rather than `NativeRendererVulkan`'s — D3D's NDC is Y-up (same as GL), unlike
 * Vulkan's Y-down NDC. See `SDFRect.hlsl`'s own comment, and `.claude/DECISIONS.md`'s Phase 35.7
 * entry, for the full derivation.
 *
 * Like `NativeRendererVulkan`, this class is constructed with explicit D3D12 handles (not routed
 * through the public `Rendering::DX12Context` — that struct exists for Phase 24 Viewport's own,
 * different, application-facing use) and needs an explicit `SetTarget()` call before its first
 * `Render()`. `Render()` allocates a fresh command allocator/list once (Phase 24's own
 * `ViewportFramebufferDX12` precedent for this exact class of resource), `Reset()`s and records
 * into them every call, then submits and `WaitForSingleObject()`s on a fence synchronously before
 * returning — the same deliberate, documented stopgap `NativeRendererVulkan::Render()` already
 * uses (and the identical stopgap `ViewportFramebufferDX12::EndRender()`'s own file comment already
 * calls out: "Phase 24 CPU stall... Replace with `ID3D12CommandQueue::Wait()`... for GPU-timeline
 * sync"). Real per-frame semaphore/fence-timeline sync is deferred to whichever later sub-phase
 * wires this renderer into `SDL3DX12Backend`'s live frame loop — this sub-phase only proves the
 * pipeline/shader/buffer mechanics work correctly in isolation, matching how
 * `NativeRendererVulkan_test.cpp`'s own first tests needed no `Application`/`Viewport` machinery
 * either.
 *
 * Phase 35.12 adds `BatchKind::Shadow`, wiring `BlurPassDX12` (Phase 35.11) into a silhouette-
 * render-then-blur-then-composite flow mirroring `NativeRendererVulkan::RenderShadowBatch()`'s own
 * Phase 35.4 structure. `BeginMainCommandList()`/`EndAndSubmitMainCommandList()` split `Render()`'s
 * previously-monolithic "reset, record everything, submit, wait" into reusable halves, mirroring
 * Vulkan's own identical `BeginMainCommandBuffer()`/`EndAndSubmitMainCommandBuffer()` split — needed
 * because `_blurPass.Apply()` is its own fully separate, synchronously-awaited submission that
 * cannot be recorded into a not-yet-submitted command list. **Unlike Vulkan** (whose
 * `VK_IMAGE_LAYOUT_GENERAL` silhouette target needs zero layout transitions across its render/blur/
 * sample roles), the silhouette resource here needs a real `RENDER_TARGET -> UNORDERED_ACCESS`
 * transition before the blur reads it, and the blur's own returned result needs a further
 * `UNORDERED_ACCESS -> PIXEL_SHADER_RESOURCE` transition before the composite draw samples it —
 * D3D12 has no single resource state valid for all three roles simultaneously the way Vulkan's
 * `GENERAL` layout is (see `BlurPassDX12.hpp`'s own file comment, Phase 35.11, which flagged this
 * exact requirement in advance). The composite draw's SRV is written into `_srvHeap` (the same heap
 * `BatchKind::Image` batches already use, Phase 35.9) at one *extra*, reserved slot — not a second
 * heap — since D3D12 only allows one `CBV_SRV_UAV` heap bound via `SetDescriptorHeaps()` at a time
 * per command list, unlike Vulkan's unlimited simultaneously-bound descriptor sets.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-13
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "BlurPassDX12.hpp"
#include "Rendering/Renderers/BatchBuilder.hpp"
#include "Rendering/Renderers/IRenderer.hpp"

#include <windows.h>
#undef CreateWindow
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

namespace ImFrame::Internal {

/**
 * @class    NativeRendererDX12
 * @brief    Renders `Rendering::DrawRect` batches via a real D3D12 graphics pipeline
 *
 * @internal
 * D3D12 resources (root signature, PSO, command allocator/list, fence, streaming vertex/index/
 * constant buffers) are created lazily on the first `Render()` call, matching
 * `NativeRendererVulkan::EnsureInitialized()`'s own lazy-initialization convention —
 * constructing an instance never issues a single D3D12 call.
 *
 * @since    3.0.1
 */
class NativeRendererDX12 final : public IRenderer {
public:
    /**
     * @brief    Constructs a renderer bound to the given (not owned) D3D12 device/queue.
     * @param[in] device       A valid, already-created `ID3D12Device4`.
     * @param[in] directQueue  A direct command queue this renderer's own command lists submit to.
     * @param[in] colorFormat  `DXGI_FORMAT` of the target resource(s) passed to `SetTarget()` —
     *                         fixed at construction time, matching this sub-phase's scope (one
     *                         target format for this renderer's whole lifetime).
     *
     * Neither handle is owned — the caller (a real backend, or a test) is responsible for their
     * lifetime outliving this object and for calling `Shutdown()` before destroying them.
     */
    NativeRendererDX12(ID3D12Device4* device, ID3D12CommandQueue* directQueue, DXGI_FORMAT colorFormat);
    ~NativeRendererDX12() override;

    NativeRendererDX12(const NativeRendererDX12&) = delete;
    NativeRendererDX12& operator=(const NativeRendererDX12&) = delete;

    /**
     * @brief    Sets the offscreen target `Render()` draws into.
     * @param[in] targetResource  An `ID3D12Resource` of format `colorFormat` (the constructor's
     *                            own parameter), created with `D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET`,
     *                            already in `D3D12_RESOURCE_STATE_RENDER_TARGET` at the time
     *                            `Render()` is called (this class does not transition it — see
     *                            this file's own comment on why there is no implicit "currently
     *                            bound" target the way `NativeRendererGL3` has via `GL_VIEWPORT`).
     * @param[in] targetRtv       A render-target-view CPU descriptor handle for `targetResource`.
     * @param[in] width           Target width, in pixels.
     * @param[in] height          Target height, in pixels.
     *
     * Must be called at least once before the first `Render()` call.
     */
    void SetTarget(ID3D12Resource* targetResource, D3D12_CPU_DESCRIPTOR_HANDLE targetRtv, std::uint32_t width,
                   std::uint32_t height) noexcept;

    void Render(const Rendering::CommandBuffer& buffer) override;

    /// Always fails — no text pipeline exists yet (this sub-phase's scope is `BatchKind::Rect` only).
    [[nodiscard]] Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) override;

    /// Releases all D3D12 resources this renderer itself created. Safe to call multiple times,
    /// including before any `Render()` call. Does not touch the constructor's borrowed handles.
    void Shutdown() override;

private:
    void EnsureInitialized();
    void EnsureVertexIndexCapacity(std::size_t vertexBytes, std::size_t indexBytes);
    void EnsureImageDescriptorCapacity(std::size_t neededSlots);
    void EnsureShadowSilhouetteTarget(std::uint32_t width, std::uint32_t height);

    /// Resets `_commandAllocator`/`_commandList` (with a `nullptr` initial PSO — each `RenderXBatch()`
    /// binds its own) and sets the main target's RTV/viewport/scissor/primitive-topology state, plus
    /// `_srvHeap` if it exists (harmless to bind even when this particular call doesn't need it) --
    /// mirrors `NativeRendererVulkan::BeginMainCommandBuffer()`.
    void BeginMainCommandList();
    /// Closes, executes, and synchronously fence-waits on the main command list -- mirrors
    /// `NativeRendererVulkan::EndAndSubmitMainCommandBuffer()`.
    void EndAndSubmitMainCommandList();

    void RenderRectBatch(const Batch& batch, std::size_t& vertexByteOffset, std::size_t& indexByteOffset);

    /**
     * @brief    Records one `BatchKind::Image` batch's draw call.
     * @param[in] srvGpuHandle  A GPU-visible descriptor-table handle into `_srvHeap`, already
     *                          written (by `Render()`, before this call, before any command-list
     *                          recording began) with `batch.Texture` reinterpreted as a raw
     *                          `ID3D12Resource*` — the D3D12 analogue of
     *                          `NativeRendererVulkan::RenderImageBatch()`'s own `imageDescriptorSet`
     *                          parameter, for the identical reason: the SRV heap's *content* is
     *                          resolved by the GPU at command-list *execution* time, so every
     *                          differently-textured `Image` batch in one `Render()` call needs its
     *                          own descriptor slot, written before recording starts.
     */
    void RenderImageBatch(D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle, const Batch& batch,
                          std::size_t& vertexByteOffset, std::size_t& indexByteOffset);

    /**
     * @brief    Renders one `BatchKind::Shadow` batch: silhouette, blur, composite (Phase 35.12).
     * @param[in] compositeSrvCpuHandle  Where to write this batch's own composite SRV in `_srvHeap`
     *                                   — one *extra* slot reserved by `Render()` beyond its own
     *                                   per-`Image`-batch slots (see this class's own file comment
     *                                   on why one shared heap, not a second one, is used).
     * @param[in] compositeSrvGpuHandle  The same slot's GPU-visible handle, bound for the composite
     *                                   draw itself.
     *
     * Mirrors `NativeRendererVulkan::RenderShadowBatch()`'s own structure: (1) ends and submits the
     * main command list as accumulated so far (`_blurPass.Apply()` is its own separate submission,
     * so everything up to this point must actually be complete on the GPU, not merely recorded),
     * (2) renders the shape's silhouette into `_shadowSilhouetteResource` via its own one-shot use
     * of `_commandList` (reusing `_rectPipelineState`), (3) calls `_blurPass.Apply()`, (4) writes the
     * blurred result's SRV and records the composite draw on a *new*, freshly-begun main command
     * list. Unlike Vulkan, steps (2)-(4) each need real `D3D12_RESOURCE_BARRIER` transitions the
     * `VK_IMAGE_LAYOUT_GENERAL` silhouette never needed — see this class's own file comment.
     */
    void RenderShadowBatch(const Batch& batch, D3D12_CPU_DESCRIPTOR_HANDLE compositeSrvCpuHandle,
                          D3D12_GPU_DESCRIPTOR_HANDLE compositeSrvGpuHandle);

    // ─── Borrowed (not owned) ──────────────────────────────────────────────────
    ID3D12Device4*      _device      = nullptr;
    ID3D12CommandQueue* _directQueue = nullptr;
    DXGI_FORMAT         _colorFormat = DXGI_FORMAT_UNKNOWN;

    // ─── Render target (set via SetTarget()) ───────────────────────────────────
    ID3D12Resource*          _targetResource = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE _targetRtv   = {};
    std::uint32_t            _targetWidth    = 0;
    std::uint32_t            _targetHeight   = 0;

    // ─── Lazily-created, owned resources ────────────────────────────────────────
    bool _initialized = false;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> _rectPipelineState;

    /// `Shaders/Image.hlsl`'s own root signature (root CBV b0 + one-SRV descriptor table t0, with
    /// a fixed static sampler s0) and PSO -- kept separate from `_rootSignature`/`_rectPipelineState`
    /// rather than shared, mirroring `NativeRendererVulkan`'s own separate `_imagePipelineLayout`/
    /// `_imagePipeline` (Phase 35.2).
    Microsoft::WRL::ComPtr<ID3D12RootSignature> _imageRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> _imagePipelineState;

    /// One shader-visible `CBV_SRV_UAV` descriptor heap, holding one SRV slot per `BatchKind::Image`
    /// batch in the current `Render()` call -- the D3D12 analogue of `NativeRendererVulkan`'s own
    /// `_imageDescriptorPool`/`_imageDescriptorSets` (Phase 35.2): grown (never shrunk), all slots
    /// written before any command-list recording begins, one draw's worth of state per slot.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _srvHeap;
    std::size_t                                  _srvHeapCapacitySlots = 0;
    UINT                                          _srvDescriptorSize    = 0;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    _commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _commandList;

    Microsoft::WRL::ComPtr<ID3D12Fence> _fence;
    std::uint64_t                       _fenceValue = 0;
    HANDLE                               _fenceEvent = nullptr;

    /// `SDFRect.hlsl`'s `cbuffer PerFrame : register(b0)` — bound directly as a root CBV (no
    /// descriptor heap needed at all for this sub-phase's scope), updated once per `Render()`
    /// call. Host-visible, persistently mapped (`D3D12_HEAP_TYPE_UPLOAD`), matching
    /// `NativeRendererVulkan`'s own `_perFrameUbo` convention.
    Microsoft::WRL::ComPtr<ID3D12Resource> _perFrameCb;
    void*                                  _perFrameCbMapped = nullptr;

    /// Streaming vertex/index buffers, re-uploaded per `Render()` call — `D3D12_HEAP_TYPE_UPLOAD`,
    /// persistently mapped, grown (never shrunk) on demand, matching `NativeRendererVulkan`'s own
    /// `_vertexBuffer`/`_indexBuffer` re-upload-every-batch convention.
    Microsoft::WRL::ComPtr<ID3D12Resource> _vertexBuffer;
    void*                                  _vertexBufferMapped        = nullptr;
    std::size_t                            _vertexBufferCapacityBytes = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> _indexBuffer;
    void*                                  _indexBufferMapped        = nullptr;
    std::size_t                            _indexBufferCapacityBytes = 0;

    // ─── Phase 35.12: BatchKind::Shadow support ─────────────────────────────────
    BlurPassDX12 _blurPass;

    /// The offscreen silhouette target — created with **both**
    /// `D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET` (for the silhouette draw) and
    /// `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS` (for `_blurPass`'s own read). At rest (between
    /// `RenderShadowBatch()` calls) it sits in `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`; each call
    /// transitions it to `RENDER_TARGET` for the silhouette draw and back, unlike Vulkan's
    /// zero-transition `GENERAL`-layout equivalent (see this class's own file comment).
    Microsoft::WRL::ComPtr<ID3D12Resource>       _shadowSilhouetteResource;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _shadowRtvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE                  _shadowRtv           = {};
    std::uint32_t                                _shadowSilhouetteWidth  = 0;
    std::uint32_t                                _shadowSilhouetteHeight = 0;

    /// Small, fixed-size, dedicated buffers for `RenderShadowBatch()`'s own two one-quad draws
    /// (silhouette rect, composite image) — kept separate from `_vertexBuffer`/`_indexBuffer` so
    /// this internal, always-6-index draw never competes with the main streaming buffers' own
    /// growth, mirroring `NativeRendererVulkan`'s own `_shadowQuadVertexBuffer`/
    /// `_shadowQuadIndexBuffer` (Phase 35.4). Sized for the *larger* of `RectVertex`/`ImageVertex`
    /// (4 vertices) since both draws reuse the same buffer, sequentially, never concurrently.
    Microsoft::WRL::ComPtr<ID3D12Resource> _shadowQuadVertexBuffer;
    void*                                  _shadowQuadVertexMapped = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> _shadowQuadIndexBuffer;
    void*                                  _shadowQuadIndexMapped  = nullptr;

    BatchBuilder _batchBuilder;
};

} // namespace ImFrame::Internal
