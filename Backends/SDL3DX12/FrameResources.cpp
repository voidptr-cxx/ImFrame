/**
 * @file     FrameResources.cpp
 * @brief    FrameFence::Create/Destroy/Signal/Wait, FrameResources::Create
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

#include "FrameResources.hpp"

namespace ImFrame::Internal {

// ─── FrameFence::Create ───────────────────────────────────────────────────────

VoidResult FrameFence::Create(ID3D12Device* device)
{
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }
    event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event) {
        fence.Reset();
        return std::unexpected(Error::GraphicsInitFailed);
    }
    value = 0;
    return {};
}

// ─── FrameFence::Destroy ──────────────────────────────────────────────────────

void FrameFence::Destroy()
{
    if (event) {
        CloseHandle(event);
        event = nullptr;
    }
    fence.Reset();
    value = 0;
}

// ─── FrameFence::Signal ───────────────────────────────────────────────────────

void FrameFence::Signal(ID3D12CommandQueue* queue, uint64_t newValue)
{
    queue->Signal(fence.Get(), newValue);
    value = newValue;
}

// ─── FrameFence::Wait ─────────────────────────────────────────────────────────

void FrameFence::Wait(uint64_t targetValue) const
{
    if (fence->GetCompletedValue() >= targetValue) return;
    fence->SetEventOnCompletion(targetValue, event);
    WaitForSingleObject(event, INFINITE);
}

// ─── FrameResources::Create ───────────────────────────────────────────────────

VoidResult FrameResources::Create(ID3D12Device4* device)
{
    if (auto r = fence.Create(device); !r) return r;

    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    // CreateCommandList1 returns the list already CLOSED — no immediate
    // Close() call needed before the first Reset(), unlike CreateCommandList.
    if (FAILED(device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                           IID_PPV_ARGS(&commandList)))) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    return {};
}

} // namespace ImFrame::Internal
