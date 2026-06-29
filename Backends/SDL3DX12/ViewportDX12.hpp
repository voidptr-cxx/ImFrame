/**
 * @file     ViewportDX12.hpp
 * @brief    DX12 IViewportFramebuffer — committed render target + descriptor handles
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"

#include <windows.h>
#undef CreateWindow
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace ImFrame::Internal {

struct DescriptorAllocator; // defined in DescriptorAllocator.hpp

/**
 * DX12 offscreen render target for Phase 24 Viewport.
 *
 * Creates a committed D3D12_HEAP_TYPE_DEFAULT resource, allocates one RTV slot
 * and one SRV slot from the backend's descriptor allocators, and exposes a
 * per-frame command list for user rendering.
 *
 * Synchronisation (Phase 24): EndRender() uses a fence + blocking WaitForSingleObject
 * (CPU stall). A proper GPU-timeline sync is a Phase 24.x optimisation.
 */
class ViewportFramebufferDX12 final : public IViewportFramebuffer {
public:
    ViewportFramebufferDX12(ID3D12Device4*        device,
                            ID3D12CommandQueue*   directQueue,
                            DescriptorAllocator*  rtvAllocator,
                            DescriptorAllocator*  srvAllocator,
                            DXGI_FORMAT           format,
                            int                   framesInFlight,
                            std::uint32_t         width,
                            std::uint32_t         height);
    ~ViewportFramebufferDX12() noexcept override;

    void Resize(std::uint32_t width, std::uint32_t height) override;
    void BeginRender(std::uint32_t frameIndex) override;
    void EndRender(std::uint32_t frameIndex) override;
    [[nodiscard]] ViewportHandles GetHandles() const override;

private:
    void Allocate(std::uint32_t width, std::uint32_t height);
    void Release() noexcept;

    struct PerFrame {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>        cmdAlloc;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7>    cmdList;
    };

    ID3D12Device4*        _device        = nullptr;
    ID3D12CommandQueue*   _directQueue   = nullptr;
    DescriptorAllocator*  _rtvAlloc      = nullptr;
    DescriptorAllocator*  _srvAlloc      = nullptr;
    DXGI_FORMAT           _format        = DXGI_FORMAT_B8G8R8A8_UNORM;

    Microsoft::WRL::ComPtr<ID3D12Resource>  _resource;
    Microsoft::WRL::ComPtr<ID3D12Fence>     _fence;
    HANDLE                                   _fenceEvent = nullptr;
    UINT64                                   _fenceValue = 0;

    UINT           _rtvSlot = UINT_MAX;
    UINT           _srvSlot = UINT_MAX;
    std::uint32_t  _width   = 0;
    std::uint32_t  _height  = 0;

    std::vector<PerFrame> _frames;
    std::uint32_t         _activeFrame = 0;
    int                   _framesInFlight = 2;

    D3D12_CPU_DESCRIPTOR_HANDLE _rtvHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE _srvGpuHandle{};
};

} // namespace ImFrame::Internal
