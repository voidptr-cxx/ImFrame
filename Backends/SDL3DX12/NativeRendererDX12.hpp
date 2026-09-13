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
 * Consumes `Shaders/SDFRect.hlsl`'s compiled DXIL directly (Phase 35.7's own
 * `cmake/CompileShaderDXC.cmake` pipeline, embedded as
 * `ImFrame::Internal::Shaders::kSDFRectVertexDxil`/`kSDFRectPixelDxil`). `SDFRect.hlsl`'s own
 * vertex shader negates Y, matching `NativeRendererGL3`'s own convention rather than
 * `NativeRendererVulkan`'s — D3D's NDC is Y-up (same as GL), unlike Vulkan's Y-down NDC. See that
 * file's own comment, and `.claude/DECISIONS.md`'s Phase 35.7 entry, for the full derivation.
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
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-13
 * @version  3.0.1
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

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
    void RenderRectBatch(const Batch& batch, std::size_t& vertexByteOffset, std::size_t& indexByteOffset);

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

    BatchBuilder _batchBuilder;
};

} // namespace ImFrame::Internal
