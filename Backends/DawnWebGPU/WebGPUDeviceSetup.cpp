/**
 * @file     WebGPUDeviceSetup.cpp
 * @brief    SetUpWebGPUDevice() — instance/adapter/device/queue creation, shared by native + Emscripten
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

#include "WebGPUDeviceSetup.hpp"

#include "ImFrame/Utility/Logger.hpp"

#include <string_view>

namespace ImFrame::Internal {

namespace {

std::string_view ToStringView(WGPUStringView sv) noexcept
{
    if (!sv.data) return {};
    return std::string_view(sv.data, sv.length);
}

const char* BackendTypeName(WGPUBackendType type) noexcept
{
    switch (type) {
        case WGPUBackendType_D3D12:    return "D3D12";
        case WGPUBackendType_D3D11:    return "D3D11";
        case WGPUBackendType_Metal:    return "Metal";
        case WGPUBackendType_Vulkan:   return "Vulkan";
        case WGPUBackendType_OpenGL:   return "OpenGL";
        case WGPUBackendType_OpenGLES: return "OpenGLES";
        case WGPUBackendType_Null:     return "Null";
        case WGPUBackendType_WebGPU:   return "WebGPU";
        default:                       return "Undefined";
    }
}

struct AdapterRequestResult { WGPUAdapter adapter = nullptr; };
struct DeviceRequestResult  { WGPUDevice  device  = nullptr; };

void OnAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                            void* userdata1, void* /*userdata2*/)
{
    auto* result = static_cast<AdapterRequestResult*>(userdata1);
    if (status == WGPURequestAdapterStatus_Success) {
        result->adapter = adapter;
    } else {
        IMF_ERROR("[WebGPU] adapter request failed: {}", ToStringView(message));
    }
}

void OnDeviceRequestEnded(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
                           void* userdata1, void* /*userdata2*/)
{
    auto* result = static_cast<DeviceRequestResult*>(userdata1);
    if (status == WGPURequestDeviceStatus_Success) {
        result->device = device;
    } else {
        IMF_ERROR("[WebGPU] device request failed: {}", ToStringView(message));
    }
}

void OnUncapturedError(WGPUDevice const* /*device*/, WGPUErrorType type, WGPUStringView message,
                        void* /*userdata1*/, void* /*userdata2*/)
{
    IMF_ERROR("[WebGPU] uncaptured error (type {}): {}", static_cast<int>(type), ToStringView(message));
}

void OnDeviceLost(WGPUDevice const* /*device*/, WGPUDeviceLostReason reason, WGPUStringView message,
                   void* /*userdata1*/, void* /*userdata2*/)
{
    if (reason == WGPUDeviceLostReason_Destroyed) {
        return; // Expected: fired by wgpuDeviceRelease() during normal shutdown.
    }
    // FailedCreation fires spontaneously when wgpuAdapterRequestDevice fails;
    // the WaitAnyOnly future still resolves via OnDeviceRequestEnded with a null device.
    // Do not assert here — the caller detects the null device and returns an error.
    IMF_ERROR("[WebGPU] device lost (reason {}): {}", static_cast<int>(reason), ToStringView(message));
}

} // anonymous namespace

VoidResult SetUpWebGPUDevice(WebGPUDeviceSetupResult& out)
{
    out = WebGPUDeviceSetupResult{};

    // WGPUInstanceWaitAny() requires BOTH:
    //   1. WGPUInstanceFeatureName_TimedWaitAny in requiredFeatures
    //   2. requiredLimits->timedWaitAnyMaxCount >= count-of-futures-per-call
    // Omitting either produces "Timeout waits are either not enabled or not supported"
    // and causes every Future-based adapter/device request to fail, even with UINT64_MAX.
    WGPUInstanceFeatureName timedWaitFeature = WGPUInstanceFeatureName_TimedWaitAny;

    WGPUInstanceLimits instanceLimits{};
    instanceLimits.timedWaitAnyMaxCount = 64;

    WGPUInstanceDescriptor instanceDesc{};
    instanceDesc.requiredFeatureCount = 1;
    instanceDesc.requiredFeatures     = &timedWaitFeature;
    instanceDesc.requiredLimits       = &instanceLimits;

    out.instance = wgpuCreateInstance(&instanceDesc);
    if (!out.instance) return std::unexpected(Error::GraphicsInitFailed);

    // ── Adapter ───────────────────────────────────────────────────────────────
    WGPURequestAdapterOptions options{};
    options.powerPreference = WGPUPowerPreference_HighPerformance;

    AdapterRequestResult adapterResult{};
    WGPURequestAdapterCallbackInfo adapterCb{};
    adapterCb.mode      = WGPUCallbackMode_WaitAnyOnly;
    adapterCb.callback  = &OnAdapterRequestEnded;
    adapterCb.userdata1 = &adapterResult;

    WGPUFuture adapterFuture = wgpuInstanceRequestAdapter(out.instance, &options, adapterCb);
    WGPUFutureWaitInfo adapterWait{};
    adapterWait.future = adapterFuture;
    wgpuInstanceWaitAny(out.instance, 1, &adapterWait, UINT64_MAX);

    if (!adapterResult.adapter) return std::unexpected(Error::GraphicsInitFailed);
    out.adapter = adapterResult.adapter;

    WGPUAdapterInfo info{};
    if (wgpuAdapterGetInfo(out.adapter, &info) == WGPUStatus_Success) {
        IMF_INFO("[WebGPU] adapter selected: {} ({}) backend={}", ToStringView(info.device),
                 ToStringView(info.description), BackendTypeName(info.backendType));
        wgpuAdapterInfoFreeMembers(info);
    }

    WGPULimits limits{};
    if (wgpuAdapterGetLimits(out.adapter, &limits) == WGPUStatus_Success) {
        out.maxTextureDimension2D = limits.maxTextureDimension2D;
    }

    // ── Device ────────────────────────────────────────────────────────────────
    DeviceRequestResult deviceResult{};
    WGPUDeviceDescriptor deviceDesc{};
    deviceDesc.deviceLostCallbackInfo.mode     = WGPUCallbackMode_AllowSpontaneous;
    deviceDesc.deviceLostCallbackInfo.callback = &OnDeviceLost;
    deviceDesc.uncapturedErrorCallbackInfo.callback = &OnUncapturedError;

    WGPURequestDeviceCallbackInfo deviceCb{};
    deviceCb.mode      = WGPUCallbackMode_WaitAnyOnly;
    deviceCb.callback  = &OnDeviceRequestEnded;
    deviceCb.userdata1 = &deviceResult;

    WGPUFuture deviceFuture = wgpuAdapterRequestDevice(out.adapter, &deviceDesc, deviceCb);
    WGPUFutureWaitInfo deviceWait{};
    deviceWait.future = deviceFuture;
    wgpuInstanceWaitAny(out.instance, 1, &deviceWait, UINT64_MAX);

    if (!deviceResult.device) return std::unexpected(Error::GraphicsInitFailed);
    out.device = deviceResult.device;
    out.queue  = wgpuDeviceGetQueue(out.device);

    return {};
}

} // namespace ImFrame::Internal
