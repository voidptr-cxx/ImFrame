/**
 * @file     WebGPUDeviceSetup.hpp
 * @brief    Shared WGPUInstance/WGPUAdapter/WGPUDevice/WGPUQueue creation logic
 *
 * Extracted out of `DawnWebGPUBackend` so the (unverified) Emscripten path —
 * `EmscriptenWebGPUBackend.cpp` — can reuse the identical instance/adapter/
 * device request sequence without duplicating it. This is the part of the
 * Phase 23 proposal's "shared WebGPU-logic" claim that actually holds: the
 * Future-based request pattern, error/device-lost callback wiring, and
 * adapter info logging are platform-independent. Window creation, surface
 * creation, and the event loop are not (see DECISIONS.md) and remain in
 * their own per-platform files.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Core/Error.hpp"

#include <cstdint>
#include <webgpu/webgpu.h>

namespace ImFrame::Internal {

/**
 * @struct  WebGPUDeviceSetupResult
 * @brief   The instance/adapter/device/queue produced by `SetUpWebGPUDevice()`
 * @since   2.3.0
 */
struct WebGPUDeviceSetupResult {
    WGPUInstance  instance              = nullptr;
    WGPUAdapter   adapter               = nullptr;
    WGPUDevice    device                = nullptr;
    WGPUQueue     queue                 = nullptr;
    std::uint32_t maxTextureDimension2D = 0;
};

/**
 * @brief    Create a WGPUInstance, request an adapter and device, and log
 *           the selected backend at `Logger::Info`.
 *
 * Blocks synchronously via `wgpuInstanceWaitAny(..., UINT64_MAX)` —
 * `wgpuInstanceRequestAdapter()`/`wgpuAdapterRequestDevice()` are
 * Future-based in this Dawn revision, not the simple callback the Phase 23
 * proposal assumed (see DECISIONS.md). The same blocking pattern is used by
 * `emdawnwebgpu` (Emscripten's WebGPU port) — verified against its shipped
 * `webgpu.h`, which declares `wgpuInstanceWaitAny` identically.
 *
 * @param[out]  out  Populated on success. Left default-constructed on failure.
 * @return   Empty result on success; an `Error` on the first failure.
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult SetUpWebGPUDevice(WebGPUDeviceSetupResult& out);

} // namespace ImFrame::Internal
