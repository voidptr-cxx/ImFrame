/**
 * @file     DescriptorAllocator.cpp
 * @brief    DescriptorAllocator::Create/Destroy/Allocate/Free/CpuHandle/GpuHandle
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-19
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "DescriptorAllocator.hpp"
#include "DX12Util.hpp"

namespace ImFrame::Internal {

// ─── Create ───────────────────────────────────────────────────────────────────

VoidResult DescriptorAllocator::Create(const DescriptorAllocatorDesc& desc)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type           = desc.heapType;
    heapDesc.NumDescriptors = desc.capacity;
    heapDesc.Flags          = desc.shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                                  : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(desc.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    descriptorSize = desc.device->GetDescriptorHandleIncrementSize(desc.heapType);
    capacity       = desc.capacity;
    nextFree       = 0;
    freeList.clear();

    if (!desc.debugName.empty()) {
        SetD3D12DebugName(heap.Get(), desc.debugName);
    }
    return {};
}

// ─── Destroy ──────────────────────────────────────────────────────────────────

void DescriptorAllocator::Destroy()
{
    heap.Reset();
    descriptorSize = 0;
    capacity       = 0;
    nextFree       = 0;
    freeList.clear();
}

// ─── Allocate ─────────────────────────────────────────────────────────────────

bool DescriptorAllocator::Allocate(UINT& outIndex) noexcept
{
    if (!freeList.empty()) {
        outIndex = freeList.back();
        freeList.pop_back();
        return true;
    }
    if (nextFree >= capacity) return false;
    outIndex = nextFree++;
    return true;
}

// ─── Free ─────────────────────────────────────────────────────────────────────

void DescriptorAllocator::Free(UINT index) noexcept
{
    if (!heap || index >= capacity) return;
    freeList.push_back(index);
}

// ─── CpuHandle ────────────────────────────────────────────────────────────────

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::CpuHandle(UINT index) const noexcept
{
    D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<SIZE_T>(index) * descriptorSize;
    return h;
}

// ─── GpuHandle ────────────────────────────────────────────────────────────────

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GpuHandle(UINT index) const noexcept
{
    D3D12_GPU_DESCRIPTOR_HANDLE h = heap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += static_cast<UINT64>(index) * descriptorSize;
    return h;
}

} // namespace ImFrame::Internal
