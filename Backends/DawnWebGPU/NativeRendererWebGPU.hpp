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
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-14
 * @version  3.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "Rendering/Renderers/BatchBuilder.hpp"
#include "Rendering/Renderers/IRenderer.hpp"

#include <webgpu/webgpu.h>

#include <cstdint>

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
     * @param[in] targetView  A `WGPUTextureView` of format `colorFormat` (the constructor's own
     *                        parameter) — the render-pass color attachment. Not owned; the caller
     *                        keeps it alive at least until the next `SetTarget()`/`Shutdown()` call.
     * @param[in] width       Target width, in pixels.
     * @param[in] height      Target height, in pixels.
     *
     * Must be called at least once before the first `Render()` call.
     */
    void SetTarget(WGPUTextureView targetView, std::uint32_t width, std::uint32_t height) noexcept;

    void Render(const Rendering::CommandBuffer& buffer) override;

    /// Always fails — no text pipeline exists yet (this sub-phase's scope is `BatchKind::Rect` only).
    [[nodiscard]] Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) override;

    /// Releases all WebGPU resources this renderer itself created. Safe to call multiple times,
    /// including before any `Render()` call. Does not touch the constructor's borrowed handles.
    void Shutdown() override;

private:
    void EnsureInitialized();
    void EnsureVertexIndexCapacity(std::size_t vertexBytes, std::size_t indexBytes);
    void RenderRectBatch(WGPURenderPassEncoder pass, const Batch& batch, std::size_t& vertexByteOffset,
                          std::size_t& indexByteOffset);

    // ─── Borrowed (not owned) ──────────────────────────────────────────────────
    WGPUDevice        _device      = nullptr;
    WGPUQueue         _queue       = nullptr;
    WGPUTextureFormat _colorFormat = WGPUTextureFormat_Undefined;

    // ─── Render target (set via SetTarget()) ───────────────────────────────────
    WGPUTextureView _targetView   = nullptr;
    std::uint32_t   _targetWidth  = 0;
    std::uint32_t   _targetHeight = 0;

    // ─── Lazily-created, owned resources ────────────────────────────────────────
    bool _initialized = false;

    WGPUShaderModule    _shaderModule    = nullptr;
    WGPUBindGroupLayout _bindGroupLayout = nullptr;
    WGPUPipelineLayout  _pipelineLayout  = nullptr;
    WGPURenderPipeline  _rectPipeline    = nullptr;

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

    BatchBuilder _batchBuilder;
};

} // namespace ImFrame::Internal
