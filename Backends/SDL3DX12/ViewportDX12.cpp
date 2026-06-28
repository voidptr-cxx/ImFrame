/**
 * @file     ViewportDX12.cpp
 * @brief    DX12 viewport framebuffer — committed resource + per-frame command lists
 *
 * Phase 24 sync: EndRender() signals a fence and calls WaitForSingleObject (CPU stall).
 * Replace with ID3D12CommandQueue::Wait() on the main submit for GPU-timeline sync.
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

#include <windows.h>
#undef CreateWindow
#include <d3d12.h>
#include <wrl/client.h>

#include "Backends/SDL3DX12/DescriptorAllocator.hpp"
#include "Backends/SDL3DX12/ViewportDX12.hpp"

namespace ImFrame::Internal {

ViewportFramebufferDX12::ViewportFramebufferDX12(
    ID3D12Device4*       device,
    ID3D12CommandQueue*  directQueue,
    DescriptorAllocator* rtvAllocator,
    DescriptorAllocator* srvAllocator,
    DXGI_FORMAT          format,
    int                  framesInFlight,
    std::uint32_t        width,
    std::uint32_t        height)
    : _device(device)
    , _directQueue(directQueue)
    , _rtvAlloc(rtvAllocator)
    , _srvAlloc(srvAllocator)
    , _format(format)
    , _framesInFlight(framesInFlight) {

    _frames.resize(static_cast<std::size_t>(framesInFlight));
    for (auto& f : _frames) {
        _device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&f.cmdAlloc));
        _device->CreateCommandList1(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&f.cmdList));
    }

    _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
    _fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    Allocate(width, height);
}

ViewportFramebufferDX12::~ViewportFramebufferDX12() noexcept {
    // Wait for all in-flight GPU work before destroying.
    if (_fence && _fenceEvent) {
        ++_fenceValue;
        _directQueue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }
    Release();
    if (_fenceEvent) { CloseHandle(_fenceEvent); _fenceEvent = nullptr; }
}

void ViewportFramebufferDX12::Resize(std::uint32_t width, std::uint32_t height) {
    // Stall GPU before destroying current resource.
    if (_fence && _fenceEvent) {
        ++_fenceValue;
        _directQueue->Signal(_fence.Get(), _fenceValue);
        if (_fence->GetCompletedValue() < _fenceValue) {
            _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
            WaitForSingleObject(_fenceEvent, INFINITE);
        }
    }
    Release();
    Allocate(width, height);
}

void ViewportFramebufferDX12::BeginRender(std::uint32_t frameIndex) {
    _activeFrame = static_cast<std::uint32_t>(frameIndex % _framesInFlight);
    auto& f = _frames[_activeFrame];
    f.cmdAlloc->Reset();
    f.cmdList->Reset(f.cmdAlloc.Get(), nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = _resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    f.cmdList->ResourceBarrier(1, &barrier);
}

void ViewportFramebufferDX12::EndRender(std::uint32_t /*frameIndex*/) {
    auto& f = _frames[_activeFrame];

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = _resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    f.cmdList->ResourceBarrier(1, &barrier);
    f.cmdList->Close();

    ID3D12CommandList* lists[] = { f.cmdList.Get() };
    _directQueue->ExecuteCommandLists(1, lists);

    ++_fenceValue;
    _directQueue->Signal(_fence.Get(), _fenceValue);
    if (_fence->GetCompletedValue() < _fenceValue) {
        _fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
        WaitForSingleObject(_fenceEvent, INFINITE); // Phase 24 CPU stall.
    }
}

ViewportHandles ViewportFramebufferDX12::GetHandles() const {
    ViewportHandles h;
    h.d3dResource = _resource.Get();
    h.d3dRTV      = static_cast<std::uintptr_t>(_rtvHandle.ptr);
    h.d3dSRV      = static_cast<std::uint64_t>(_srvGpuHandle.ptr);
    h.d3dCmdList  = _frames.empty() ? nullptr : _frames[_activeFrame].cmdList.Get();
    h.width       = _width;
    h.height      = _height;
    h.imTextureId = static_cast<std::uint64_t>(_srvGpuHandle.ptr);
    return h;
}

void ViewportFramebufferDX12::Allocate(std::uint32_t width, std::uint32_t height) {
    _width  = width;
    _height = height;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width            = width;
    desc.Height           = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels        = 1;
    desc.Format           = _format;
    desc.SampleDesc.Count = 1;
    desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearVal{};
    clearVal.Format   = _format;
    clearVal.Color[3] = 1.0f;

    _device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearVal, IID_PPV_ARGS(&_resource));

    // RTV descriptor.
    UINT rtvSlot = UINT_MAX;
    _rtvAlloc->Allocate(rtvSlot);
    _rtvSlot = rtvSlot;
    _rtvHandle = { _rtvAlloc->heap->GetCPUDescriptorHandleForHeapStart().ptr
                   + static_cast<SIZE_T>(rtvSlot) * _rtvAlloc->descriptorSize };
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format             = _format;
    rtvDesc.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
    _device->CreateRenderTargetView(_resource.Get(), &rtvDesc, _rtvHandle);

    // SRV descriptor (shader-visible heap).
    UINT srvSlot = UINT_MAX;
    _srvAlloc->Allocate(srvSlot);
    _srvSlot = srvSlot;
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = {
        _srvAlloc->heap->GetCPUDescriptorHandleForHeapStart().ptr
        + static_cast<SIZE_T>(srvSlot) * _srvAlloc->descriptorSize };
    _srvGpuHandle = { _srvAlloc->heap->GetGPUDescriptorHandleForHeapStart().ptr
                      + static_cast<UINT64>(srvSlot) * _srvAlloc->descriptorSize };

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                    = _format;
    srvDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels       = 1;
    _device->CreateShaderResourceView(_resource.Get(), &srvDesc, srvCpu);
}

void ViewportFramebufferDX12::Release() noexcept {
    _resource.Reset();
    if (_rtvAlloc && _rtvSlot != UINT_MAX) {
        _rtvAlloc->Free(_rtvSlot);
        _rtvSlot = UINT_MAX;
        _rtvHandle = {};
    }
    if (_srvAlloc && _srvSlot != UINT_MAX) {
        _srvAlloc->Free(_srvSlot);
        _srvSlot = UINT_MAX;
        _srvGpuHandle = {};
    }
}

} // namespace ImFrame::Internal
