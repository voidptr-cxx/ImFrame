/**
 * @file     DescriptorAllocator.hpp
 * @brief    Free-list allocator for slots within a single D3D12 descriptor heap
 *
 * Owns one `ID3D12DescriptorHeap` (RTV or shader-visible CBV_SRV_UAV) and hands
 * out fixed-size slot indices from it. `Allocate()` reuses a freed index before
 * growing the high-water mark; `Free()` returns an index to the free list
 * without shrinking the heap. One instance per heap — the backend owns two
 * (RTV, SRV/CBV/UAV), per the Phase 22 proposal's Descriptor Heaps section.
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

#include "ImFrame/Core/Error.hpp"

#include <windows.h>
#undef CreateWindow // see DX12Util.hpp's comment on this macro collision

#include <d3d12.h>
#include <string_view>
#include <vector>
#include <wrl/client.h>

namespace ImFrame::Internal {

/**
 * @struct DescriptorAllocatorDesc
 * @brief  Parameters for `DescriptorAllocator::Create()`
 * @since  2.2.0
 */
struct DescriptorAllocatorDesc {
    ID3D12Device*              device        = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE heapType      = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    UINT                       capacity      = 1024;
    bool                       shaderVisible = false;
    std::string_view           debugName;
};

/**
 * @struct DescriptorAllocator
 * @brief  Owns a descriptor heap and a free-list slot allocator over it
 *
 * @since 2.2.0
 */
struct DescriptorAllocator {
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    UINT descriptorSize = 0; ///< GetDescriptorHandleIncrementSize() for this heap's type.
    UINT capacity        = 0;
    UINT nextFree         = 0; ///< High-water mark — slots [0, nextFree) have been issued at least once.
    std::vector<UINT> freeList; ///< Recycled slots below nextFree, available for reuse.

    /**
     * @brief   Create the descriptor heap.
     *
     * @param[in]  desc  Creation parameters.
     * @return  Empty result on success; an Error on failure.
     * @throws  Nothing.
     */
    VoidResult Create(const DescriptorAllocatorDesc& desc);

    /**
     * @brief   Release the heap. Safe to call on a default-constructed instance.
     */
    void Destroy();

    /**
     * @brief   Allocate one descriptor slot.
     *
     * @param[out] outIndex  Receives the allocated slot index on success.
     * @return  `true` if a slot was available; `false` if the heap is full.
     * @throws  Nothing.
     */
    [[nodiscard]] bool Allocate(UINT& outIndex) noexcept;

    /**
     * @brief   Return a previously allocated slot to the free list.
     *
     * @param[in]  index  Slot index from a prior `Allocate()` call.
     * @throws  Nothing.
     */
    void Free(UINT index) noexcept;

    /// CPU-visible handle for the given slot index.
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(UINT index) const noexcept;

    /// GPU-visible handle for the given slot index. Only meaningful when `shaderVisible` was true at Create().
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(UINT index) const noexcept;
};

} // namespace ImFrame::Internal
