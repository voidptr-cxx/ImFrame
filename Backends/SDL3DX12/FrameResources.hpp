/**
 * @file     FrameResources.hpp
 * @brief    Per-frame-in-flight D3D12 synchronisation and command recording resources
 *
 * `FrameFence` is the named-wrapper pattern established by Phase 21's
 * `FrameSemaphore`, adapted to D3D12's fence model: `Signal()` records a new
 * target value on a queue; `Wait()` blocks the CPU only if the GPU has not
 * yet reached that value (`ID3D12Fence::GetCompletedValue()` makes the common
 * case — already reached — a non-blocking check).
 *
 * `FrameResources` bundles one `FrameFence` with the per-slot command
 * allocator and command list. The command list is created once (via
 * `CreateCommandList1`, which returns it already in the *closed* state) and
 * `Reset()` + re-recorded every frame, unlike Vulkan's per-frame command
 * buffer reallocation from a reset pool.
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

#pragma once

#include "ImFrame/Core/Error.hpp"

#include <windows.h>
#undef CreateWindow // see DX12Util.hpp's comment on this macro collision

#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

namespace ImFrame::Internal {

/**
 * @struct FrameFence
 * @brief  Owns one `ID3D12Fence` and its CPU-side waitable event
 *
 * Move-only — the waitable `HANDLE` is not RAII-managed by `ComPtr` and must
 * be explicitly transferred or closed.
 *
 * @since 2.2.0
 */
struct FrameFence {
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    HANDLE   event = nullptr; ///< Created via CreateEvent(nullptr, FALSE, FALSE, nullptr).
    uint64_t value = 0;       ///< Next value this fence will be signalled to.

    FrameFence() = default;
    ~FrameFence() { Destroy(); }

    FrameFence(const FrameFence&)            = delete;
    FrameFence& operator=(const FrameFence&) = delete;

    FrameFence(FrameFence&& other) noexcept
        : fence(std::move(other.fence)), event(other.event), value(other.value)
    {
        other.event = nullptr;
    }

    FrameFence& operator=(FrameFence&& other) noexcept
    {
        if (this != &other) {
            Destroy();
            fence       = std::move(other.fence);
            event       = other.event;
            value       = other.value;
            other.event = nullptr;
        }
        return *this;
    }

    /**
     * @brief   Create the fence (initial value 0) and its waitable event.
     *
     * @param[in]  device  Owning D3D12 device.
     * @return  Empty result on success; an Error on failure.
     * @throws  Nothing.
     */
    VoidResult Create(ID3D12Device* device);

    /**
     * @brief   Release the fence and close the event handle.
     *
     * Safe to call on a default-constructed or already-destroyed instance.
     */
    void Destroy();

    /**
     * @brief   Record a new target value on the given queue's timeline.
     *
     * @param[in]  queue     Queue to signal this fence from.
     * @param[in]  newValue  The value this fence will reach once the queue's
     *                       prior work completes. Must be monotonically increasing.
     */
    void Signal(ID3D12CommandQueue* queue, uint64_t newValue);

    /**
     * @brief   Block the calling thread until the fence reaches `targetValue`.
     *
     * Non-blocking (just a `GetCompletedValue()` check) if the GPU has
     * already reached `targetValue`.
     *
     * @param[in]  targetValue  The value to wait for.
     */
    void Wait(uint64_t targetValue) const;
};

/**
 * @struct FrameResources
 * @brief  D3D12 objects owned by a single frame-in-flight slot for one window
 *
 * @since 2.2.0
 */
struct FrameResources {
    FrameFence fence;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     commandAllocator; ///< Type DIRECT.
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7> commandList;      ///< Created once; Reset() each frame.

    /**
     * @brief   Create the fence, command allocator, and command list for this slot.
     *
     * @param[in]  device  Owning D3D12 device. Must support ID3D12Device4
     *                      (CreateCommandList1()).
     * @return  Empty result on success; an Error on failure.
     * @throws  Nothing.
     */
    VoidResult Create(ID3D12Device4* device);
};

} // namespace ImFrame::Internal
