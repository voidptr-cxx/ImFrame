/**
 * @file     BlurPassDX12.hpp
 * @brief    Separable Gaussian blur for `NativeRendererDX12`, implemented as two compute-shader passes
 *
 * @internal
 * The DX12 counterpart to `BlurPassVulkan` (Phase 35.3) — mirrors its public shape (`Apply()`/
 * `Shutdown()`, lazy initialization, reallocate-ping-pong-targets-only-on-size-change) but via two
 * `Dispatch()` compute passes against `Shaders/GaussianBlur.hlsl`'s compiled DXIL. See that file's
 * own header comment for why the Gaussian weight formula is computed inline (matching
 * `BlurPassVulkan`'s/`BlurPassGL3`'s identical choice) rather than precomputed into a buffer.
 *
 * Unlike Vulkan, D3D12 has no single resource *state* valid for both "render target" and "compute
 * UAV" roles the way Vulkan's `VK_IMAGE_LAYOUT_GENERAL` covers both a colour attachment and a
 * storage image simultaneously — `D3D12_RESOURCE_STATE_RENDER_TARGET` and
 * `D3D12_RESOURCE_STATE_UNORDERED_ACCESS` are mutually exclusive states requiring an explicit
 * transition barrier between them. `BlurPassDX12`'s own ping-pong targets are created directly in,
 * and stay in, `D3D12_RESOURCE_STATE_UNORDERED_ACCESS` for their whole lifetime (they are never
 * rendered into), so this only matters to whichever future sub-phase wires this pass into
 * `NativeRendererDX12::RenderShadowBatch()` — its own silhouette render target will need an
 * explicit `RENDER_TARGET -> UNORDERED_ACCESS` transition before `Apply()`, and
 * `UNORDERED_ACCESS -> PIXEL_SHADER_RESOURCE` before the composite draw samples the result,
 * unlike `NativeRendererVulkan::RenderShadowBatch()`, which needs neither.
 *
 * `Apply()` is entirely self-contained: it allocates its own one-shot command list, records both
 * passes (with the mandatory UAV barrier between them — `Dispatch()` calls give no implicit
 * ordering guarantee for the *data* one writes and the next reads, unlike a render pass's implicit
 * ordering), submits, and blocks on a fence synchronously before returning — the same deliberate,
 * documented stopgap `NativeRendererDX12::Render()` already uses (see that class's own file
 * comment).
 *
 * `Apply()`'s `source` is a precondition, not something this class manages: the caller must
 * already have it in `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`. `Apply()` never transitions the
 * source resource itself, only its own owned ping-pong targets (which need no transition at all,
 * since they're created directly in, and never leave, `UNORDERED_ACCESS`).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-09-15
 * @version  3.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

// NOMINMAX before windows.h -- Apply() below calls std::min(), which windows.h's own min/max
// macros would otherwise mangle (the same pre-existing, project-wide gap flagged against
// Tests/Backends/DX12Conformance_test.cpp, Phase 35.8's own DECISIONS.md entry); fixed locally
// here rather than project-wide, since this file is what actually needs it.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef CreateWindow
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

namespace ImFrame::Internal {

/// A blurred result is just the resource — unlike Vulkan's `BlurResult{Image; View}` pair, D3D12
/// has no persistent "image view" object; a UAV/SRV descriptor is created on demand from a
/// resource whenever one is actually needed (matching `NativeRendererDX12`'s own established
/// "`TextureId::Value()` is a raw `ID3D12Resource*`" convention, Phase 35.9).
struct BlurResult {
    ID3D12Resource* Resource = nullptr;
};

/**
 * @class    BlurPassDX12
 * @brief    Applies a separable Gaussian blur to a `DXGI_FORMAT_R8G8B8A8_UNORM` resource via two compute-shader ping-pong passes
 *
 * @internal
 * D3D12 resources (root signature, PSO, UAV descriptor heap, ping-pong textures) are created
 * lazily on the first `Apply()` call, matching `NativeRendererDX12::EnsureInitialized()`'s own
 * lazy-initialization convention — constructing an instance never issues a single D3D12 call.
 *
 * @since    3.2.0
 */
class BlurPassDX12 {
public:
    /**
     * @brief    Constructs a blur pass bound to the given (not owned) D3D12 device/queue.
     * @param[in] device       A valid, already-created `ID3D12Device4`.
     * @param[in] directQueue  A direct command queue this pass's own command lists submit to
     *                         (a direct queue supports compute dispatches; a dedicated compute
     *                         queue is not needed for this sub-phase's scope).
     *
     * Neither handle is owned — the caller is responsible for their lifetime outliving this
     * object and for calling `Shutdown()` before destroying them.
     */
    BlurPassDX12(ID3D12Device4* device, ID3D12CommandQueue* directQueue);
    ~BlurPassDX12();

    BlurPassDX12(const BlurPassDX12&)            = delete;
    BlurPassDX12& operator=(const BlurPassDX12&) = delete;

    /**
     * @brief    Blurs `source` with a separable Gaussian kernel and returns the result.
     * @param[in] source  The resource to blur. Not modified. Must already be
     *                    `DXGI_FORMAT_R8G8B8A8_UNORM`, exactly `width` x `height`, created with
     *                    `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS`, and already in
     *                    `D3D12_RESOURCE_STATE_UNORDERED_ACCESS` — see this class's own file
     *                    comment on why `Apply()` does not transition it itself.
     * @param[in] width   Resource width, in texels. Must match `source`'s actual width.
     * @param[in] height  Resource height, in texels. Must match `source`'s actual height.
     * @param[in] radius  Blur radius in pixels, clamped to `[0, 64]`. `<= 0` is a no-op.
     * @return   `source` unchanged if `radius <= 0`; otherwise a `DXGI_FORMAT_R8G8B8A8_UNORM`
     *           result (in `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`) owned by this `BlurPassDX12`,
     *           valid until the next `Apply()` call or destruction. The caller must not release it.
     */
    [[nodiscard]] BlurResult Apply(BlurResult source, std::uint32_t width, std::uint32_t height, float radius);

    /// Releases all D3D12 resources this pass itself created. Safe to call multiple times,
    /// including before any `Apply()` call. Does not touch the constructor's borrowed handles.
    void Shutdown();

private:
    void EnsureInitialized();
    void EnsureTargets(std::uint32_t width, std::uint32_t height);
    void RecordPass(D3D12_GPU_DESCRIPTOR_HANDLE tableBase, std::uint32_t width, std::uint32_t height,
                    float directionX, float directionY, float radius);

    // ─── Borrowed (not owned) ──────────────────────────────────────────────────
    ID3D12Device4*      _device      = nullptr;
    ID3D12CommandQueue* _directQueue = nullptr;

    // ─── Lazily-created, owned resources ────────────────────────────────────────
    bool _initialized = false;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> _rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> _pipelineState;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    _commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> _commandList;
    Microsoft::WRL::ComPtr<ID3D12Fence>               _fence;
    std::uint64_t                                     _fenceValue = 0;
    HANDLE                                             _fenceEvent = nullptr;

    /// One shader-visible `CBV_SRV_UAV` heap, 4 slots: [0]=this call's own `source` (rewritten
    /// every `Apply()` call), [1]=`_imageA` (fixed once allocated), [2]=`_imageA` again (pass 2's
    /// own source), [3]=`_imageB` (fixed) — laid out so pass 1's descriptor table (slots 0-1) and
    /// pass 2's (slots 2-3) are each two contiguous descriptors, matching
    /// `Shaders/GaussianBlur.hlsl`'s own `u0`/`u1` pair. Mirrors `BlurPassVulkan`'s own
    /// `_descriptorSetA`/`_descriptorSetB` split, adapted to D3D12's descriptor-heap model.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> _uavHeap;
    UINT                                          _uavDescriptorSize = 0;

    /// Ping-pong targets: pass 1 (horizontal) reads the caller's `source`, writes `_imageA`; pass 2
    /// (vertical) reads `_imageA`, writes `_imageB` (the resource `Apply()` returns). Reallocated
    /// only when the requested size differs from `_targetWidth`/`_targetHeight`; created directly
    /// in, and kept in, `D3D12_RESOURCE_STATE_UNORDERED_ACCESS` for their whole lifetime (see this
    /// file's own comment on why no per-pass transition is needed for these two specifically).
    Microsoft::WRL::ComPtr<ID3D12Resource> _imageA;
    Microsoft::WRL::ComPtr<ID3D12Resource> _imageB;
    std::uint32_t                          _targetWidth  = 0;
    std::uint32_t                          _targetHeight = 0;
};

} // namespace ImFrame::Internal
