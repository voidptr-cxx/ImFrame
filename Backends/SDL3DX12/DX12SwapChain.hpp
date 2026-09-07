/**
 * @file     DX12SwapChain.hpp
 * @brief    DXGI swap chain encapsulation for a single SDL3/Win32 window
 *
 * `DX12SwapChain` mirrors the Phase 20 `SwapChain` in responsibility: owns the
 * `IDXGISwapChain4`, its back-buffer resources, and their RTV descriptors.
 * Resize is a `ResizeBuffers()` call after draining in-flight frames — no full
 * recreation, unlike Vulkan's `oldSwapchain`-based swap chain replacement.
 *
 * RTV descriptors are allocated from a shared `DescriptorAllocator` (the
 * backend's RTV heap) passed in by the owner, not created in a private heap —
 * per the Phase 22 proposal's Descriptor Heaps section, one RTV heap is shared
 * across all windows.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-19
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "DescriptorAllocator.hpp"

#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Core/Error.hpp"

#include <windows.h>
#undef CreateWindow // see DX12Util.hpp's comment on this macro collision

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <wrl/client.h>

namespace ImFrame::Internal {

/**
 * @struct DX12SwapChainDesc
 * @brief  Parameters for `DX12SwapChain::Create()`
 * @since  2.2.0
 */
struct DX12SwapChainDesc {
    IDXGIFactory4*       factory          = nullptr; ///< Backend's DXGI factory (>= version 4).
    ID3D12Device*        device           = nullptr;
    ID3D12CommandQueue*  directQueue      = nullptr; ///< CreateSwapChainForHwnd requires the queue that presents.
    HWND                 hwnd             = nullptr;
    VSyncMode            vsyncMode        = VSyncMode::On;
    bool                 hdrOutput        = false;
    bool                 tearingSupported = false;    ///< From IDXGIFactory5::CheckFeatureSupport, queried once by the backend.
    int                  framesInFlight   = 2;
    int                  drawableWidth    = 0;
    int                  drawableHeight   = 0;
    DescriptorAllocator* rtvAllocator     = nullptr;  ///< Shared RTV heap; one slot consumed per back buffer.
};

/**
 * @struct DX12SwapChain
 * @brief  Owns an `IDXGISwapChain4`, its back buffers, and their RTV descriptors
 *
 * @since 2.2.0
 */
struct DX12SwapChain {
    Microsoft::WRL::ComPtr<IDXGISwapChain4> handle;
    DXGI_FORMAT format          = DXGI_FORMAT_B8G8R8A8_UNORM;
    UINT        bufferCount     = 0;
    UINT        width           = 0;
    UINT        height          = 0;
    bool        tearingEnabled  = false;
    /// Not closed manually — owned and released by the swap chain itself per MSDN.
    HANDLE      frameLatencyWaitableObject = nullptr;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> buffers;
    std::vector<UINT> rtvIndices; ///< Slot indices into `rtvAllocator`, one per buffer.

    DescriptorAllocator* rtvAllocator = nullptr; ///< Not owned — borrowed from the backend for Resize()/Destroy().
    ID3D12Device*        device       = nullptr; ///< Not owned — needed by CreateRenderTargetView() in Resize().

    /**
     * @brief   Create the swap chain, its back buffers, and their RTV descriptors.
     *
     * @param[in]  desc  Creation parameters.
     * @return  Empty result on success; an Error on failure.
     * @throws  Nothing.
     */
    VoidResult Create(const DX12SwapChainDesc& desc);

    /**
     * @brief   Release back buffers, their RTV slots, and the swap chain itself.
     *
     * Safe to call on a default-constructed instance. Caller must have already
     * drained all in-flight frames referencing the back buffers.
     */
    void Destroy();

    /**
     * @brief   Resize the swap chain's back buffers.
     *
     * Caller must have already drained all in-flight frames before calling —
     * `ResizeBuffers()` fails if any back buffer is still referenced by a
     * pending command list.
     *
     * @param[in]  newWidth   New width in pixels.
     * @param[in]  newHeight  New height in pixels.
     * @return  Empty result on success; an Error on failure (the old buffers
     *          are released either way and must not be used again).
     * @throws  Nothing.
     */
    VoidResult Resize(UINT newWidth, UINT newHeight);

    /// CPU-visible RTV handle for the current back buffer (`GetCurrentBackBufferIndex()`).
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const noexcept;

    /// The `ID3D12Resource*` for the current back buffer.
    [[nodiscard]] ID3D12Resource* CurrentBuffer() const noexcept;

private:
    VoidResult CreateBackBufferViews();
    void       ReleaseBackBufferViews();
};

} // namespace ImFrame::Internal
