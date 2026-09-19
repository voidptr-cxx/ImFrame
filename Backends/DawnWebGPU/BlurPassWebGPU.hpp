/**
 * @file     BlurPassWebGPU.hpp
 * @brief    Separable Gaussian blur for `NativeRendererWebGPU`, implemented as two compute-shader ping-pong passes
 *
 * @internal
 * Phase 35.15 — the WebGPU counterpart to `BlurPassVulkan` (Phase 35.3) and `BlurPassDX12` (Phase
 * 35.11): two `wgpuComputePassEncoderDispatchWorkgroups()` passes against an inline WGSL compute
 * shader (compiled at runtime by Dawn, matching `NativeRendererWebGPU`'s own established "no
 * offline compile/embed pipeline" convention, Phase 35.8) against `GaussianBlur.hlsl`'s/`.glsl`'s
 * identical weighted-kernel math.
 *
 * Unlike Vulkan (`imageLoad`/`imageStore` against a `VK_IMAGE_LAYOUT_GENERAL` storage image, one
 * mutable descriptor set rewritten per call) or DX12 (`RWTexture2D` UAVs, an explicit
 * `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`-at-rest resource), this pass's source texture is bound
 * as a **read-only** `texture_storage_2d<rgba8unorm, read>` (the destination as a **write-only**
 * `texture_storage_2d<rgba8unorm, write>`) — WGSL's own separate read/write storage-texture access
 * modes, needing no manual layout/state transitions at all: WebGPU exposes no explicit resource-
 * state concept to the API surface in the first place (Phase 35.10's own finding, "WebGPU's own API
 * surface is consistently simpler than Vulkan's/DX12's," confirmed again here) — the implementation
 * inserts whatever synchronization successive passes reading/writing the same resource need,
 * automatically, including between this pass's own two dispatches and later, when a caller (e.g. a
 * future `RenderShadowBatch()`) samples this pass's own returned result as a plain `texture_2d<f32>`
 * with no explicit "make visible for sampling" step required.
 *
 * A `WGPUBindGroup` is immutable once created (Phase 35.10's own finding) — rather than mirroring
 * Vulkan's two descriptor sets (one mutated in place per `Apply()` call for its own varying `source`
 * binding), this pass creates a **fresh** bind group for pass 1 every `Apply()` call (referencing
 * that call's own `source` view) and reuses one long-lived bind group for pass 2 (`_imageA ->
 * _imageB`, stable across calls at a given size, recreated only in `EnsureTargets()`), matching
 * `RenderImageBatch()`'s own "create fresh where it varies, no capacity bookkeeping" idiom.
 *
 * WebGPU has no push-constant/root-constant equivalent (Phase 35.8's own finding) — `Direction`/
 * `Radius` are passed via two small, **separate** uniform buffers (`_pass1ConstantsBuffer`/
 * `_pass2ConstantsBuffer`), each written once via `wgpuQueueWriteBuffer()` before either dispatch is
 * recorded. A *single* shared buffer, rewritten between the two dispatches, would not work: a
 * `wgpuQueueWriteBuffer()` call is itself a queued operation, and the queue processes queued
 * operations (writes and submits) in FIFO program order — a second write enqueued before this
 * call's own `wgpuQueueSubmit()` would complete *before* the submitted command buffer executes at
 * all, leaving the shared buffer holding only its own last-written value throughout both dispatches
 * (the same "content resolves at execution time" hazard this codebase's per-batch descriptor writes
 * already guard against elsewhere, Phase 35.2/35.9). Two disjoint buffers, each written exactly
 * once, sidesteps this entirely.
 *
 * `Apply()` is entirely self-contained: it creates its own command encoder, records both compute
 * dispatches into one compute pass, finishes, and submits — no fence or CPU stall of any kind is
 * needed, matching `NativeRendererWebGPU::Render()`'s own identical finding ("WebGPU's sequential
 * submit model guarantees this submit's work completes before the next one on this queue").
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-19
 * @version  3.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <webgpu/webgpu.h>

#include <cstdint>

namespace ImFrame::Internal {

/// A blurred result is both handles together, mirroring Vulkan's own `{Image; View}` pair for the
/// identical reason: a `WGPUTextureView` alone can't be copied (`wgpuCommandEncoderCopyTextureTo-
/// Texture()`/`CopyTextureToBuffer()` both take a `WGPUTexture`, not a view) by a future caller
/// (e.g. a `BackdropBlur` composite's own region copy) afterward, even though WebGPU exposes no
/// explicit resource-state model to transition (Phase 35.10's own finding still holds for *that*
/// part — no layout/state field is needed here, unlike Vulkan's `VkImageLayout`/DX12's
/// `D3D12_RESOURCE_STATES`). A composite draw sampling this result only ever needs `View`; `Texture`
/// exists solely for a future copy-based consumer.
struct BlurResult {
    WGPUTexture     Texture = nullptr;
    WGPUTextureView View    = nullptr;
};

class BlurPassWebGPU {
public:
    /**
     * @brief    Constructs a blur pass bound to the given (not owned) WebGPU device/queue.
     * @param[in] device  A valid, already-created `WGPUDevice`.
     * @param[in] queue   This device's own `WGPUQueue`.
     *
     * Neither handle is owned — the caller is responsible for their lifetime outliving this
     * object and for calling `Shutdown()` before destroying them.
     */
    BlurPassWebGPU(WGPUDevice device, WGPUQueue queue);
    ~BlurPassWebGPU();

    BlurPassWebGPU(const BlurPassWebGPU&)            = delete;
    BlurPassWebGPU& operator=(const BlurPassWebGPU&) = delete;

    /**
     * @brief    Blurs `source` with a separable Gaussian kernel and returns the result.
     * @param[in] source  The texture to blur, by view. Not modified. Must already be
     *                    `WGPUTextureFormat_RGBA8Unorm`, exactly `width` x `height`, with usage
     *                    including `WGPUTextureUsage_StorageBinding`. `Apply()` never transitions
     *                    or otherwise touches `source` itself, only its own owned ping-pong
     *                    targets.
     * @param[in] width   Texture width, in texels. Must match `source`'s actual width.
     * @param[in] height  Texture height, in texels. Must match `source`'s actual height.
     * @param[in] radius  Blur radius in pixels, clamped to `[0, 64]`. `<= 0` is a no-op.
     * @return   `source` unchanged if `radius <= 0`; otherwise a `WGPUTextureFormat_RGBA8Unorm`
     *           result view (usage including `WGPUTextureUsage_TextureBinding` so a caller can
     *           sample it directly) owned by this `BlurPassWebGPU`, valid until the next `Apply()`
     *           call or destruction. The caller must not release it.
     */
    [[nodiscard]] BlurResult Apply(BlurResult source, std::uint32_t width, std::uint32_t height, float radius);

    /// Releases all WebGPU resources this pass itself created. Safe to call multiple times,
    /// including before any `Apply()` call. Does not touch the constructor's borrowed handles.
    void Shutdown();

private:
    void EnsureInitialized();
    void EnsureTargets(std::uint32_t width, std::uint32_t height);

    // ─── Borrowed (not owned) ──────────────────────────────────────────────────
    WGPUDevice _device = nullptr;
    WGPUQueue  _queue  = nullptr;

    // ─── Lazily-created, owned resources ────────────────────────────────────────
    bool _initialized = false;

    WGPUShaderModule    _computeModule    = nullptr;
    WGPUBindGroupLayout _bindGroupLayout  = nullptr;
    WGPUPipelineLayout  _pipelineLayout   = nullptr;
    WGPUComputePipeline _computePipeline  = nullptr;

    /// Ping-pong targets: pass 1 (horizontal) reads the caller's `source`, writes `_textureA`;
    /// pass 2 (vertical) reads `_textureA`, writes `_textureB` (the view `Apply()` returns).
    /// Reallocated only when the requested size differs from `_targetWidth`/`_targetHeight`.
    WGPUTexture     _textureA    = nullptr;
    WGPUTextureView _viewA       = nullptr;
    WGPUTexture     _textureB    = nullptr;
    WGPUTextureView _viewB       = nullptr;
    std::uint32_t   _targetWidth  = 0;
    std::uint32_t   _targetHeight = 0;

    /// Pass 2's own bind group (`_viewA` -> `_viewB`) is stable across `Apply()` calls at a given
    /// size — recreated only in `EnsureTargets()`, alongside `_textureA`/`_textureB` themselves.
    /// Pass 1's own bind group varies every call (its own `source` binding), so it is created fresh
    /// inline in `Apply()` instead, matching `RenderImageBatch()`'s own identical idiom.
    WGPUBindGroup _pass2BindGroup = nullptr;

    /// See this class's own file comment on why `Direction`/`Radius` need two separate, disjoint
    /// uniform buffers rather than one buffer rewritten between passes.
    WGPUBuffer _pass1ConstantsBuffer = nullptr;
    WGPUBuffer _pass2ConstantsBuffer = nullptr;
};

} // namespace ImFrame::Internal
