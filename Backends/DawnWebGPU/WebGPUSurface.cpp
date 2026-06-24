/**
 * @file     WebGPUSurface.cpp
 * @brief    WebGPUSurface::Configure/Resize/AcquireCurrentTexture/Present/Destroy
 *
 * @internal
 * Status classification in `AcquireCurrentTexture()` is written directly
 * against the real `WGPUSurfaceGetCurrentTextureStatus` enum rather than via
 * imgui's `ImGui_ImplWGPU_IsSurfaceStatusError()`/`IsSurfaceStatusSubOptimal()`
 * helpers — those helpers branch on `IMGUI_IMPL_WEBGPU_BACKEND_DAWN` being
 * defined when vcpkg's imgui port itself was *built* (confirmed it is, via
 * `imgui/CMakeLists.txt`), but depending on that external build-time detail
 * for our own status classification is an unnecessary coupling. See
 * DECISIONS.md.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "WebGPUSurface.hpp"

namespace ImFrame::Internal {

namespace {

WGPUPresentMode ChoosePresentMode(const WGPUSurfaceCapabilities& caps, VSyncMode requested)
{
    auto supports = [&](WGPUPresentMode mode) {
        for (size_t i = 0; i < caps.presentModeCount; ++i) {
            if (caps.presentModes[i] == mode) return true;
        }
        return false;
    };

    switch (requested) {
        case VSyncMode::Off:
            return supports(WGPUPresentMode_Immediate) ? WGPUPresentMode_Immediate : WGPUPresentMode_Fifo;
        case VSyncMode::Adaptive:
            // Falls back to On (Fifo) when FifoRelaxed is unavailable — same fallback chain as Phase 22.
            return supports(WGPUPresentMode_FifoRelaxed) ? WGPUPresentMode_FifoRelaxed : WGPUPresentMode_Fifo;
        case VSyncMode::On:
        default:
            return WGPUPresentMode_Fifo; // Guaranteed present per WGPUSurfaceCapabilities contract.
    }
}

WGPUTextureFormat ChooseFormat(const WGPUSurfaceCapabilities& caps)
{
    for (size_t i = 0; i < caps.formatCount; ++i) {
        if (caps.formats[i] == WGPUTextureFormat_BGRA8UnormSrgb) return WGPUTextureFormat_BGRA8UnormSrgb;
    }
    for (size_t i = 0; i < caps.formatCount; ++i) {
        if (caps.formats[i] == WGPUTextureFormat_BGRA8Unorm) return WGPUTextureFormat_BGRA8Unorm;
    }
    return caps.formatCount > 0 ? caps.formats[0] : WGPUTextureFormat_BGRA8Unorm;
}

} // namespace

// ─── Move ───────────────────────────────────────────────────────────────────

WebGPUSurface::WebGPUSurface(WebGPUSurface&& other) noexcept
    : _surface(other._surface), _instance(other._instance), _adapter(other._adapter), _device(other._device),
      _format(other._format), _presentMode(other._presentMode), _width(other._width), _height(other._height)
{
    other._surface = nullptr;
}

WebGPUSurface& WebGPUSurface::operator=(WebGPUSurface&& other) noexcept
{
    if (this != &other) {
        Destroy();
        _surface     = other._surface;
        _instance    = other._instance;
        _adapter     = other._adapter;
        _device      = other._device;
        _format      = other._format;
        _presentMode = other._presentMode;
        _width       = other._width;
        _height      = other._height;
        other._surface = nullptr;
    }
    return *this;
}

// ─── Configure ────────────────────────────────────────────────────────────────

VoidResult WebGPUSurface::Configure(WGPUSurface surface, WGPUInstance instance, WGPUAdapter adapter,
                                     WGPUDevice device, std::uint32_t width, std::uint32_t height, VSyncMode vsync)
{
    if (!surface) return std::unexpected(Error::GraphicsInitFailed);

    _surface  = surface;
    _instance = instance;
    _adapter  = adapter;
    _device   = device;

    WGPUSurfaceCapabilities caps{};
    if (wgpuSurfaceGetCapabilities(_surface, _adapter, &caps) != WGPUStatus_Success) {
        return std::unexpected(Error::GraphicsInitFailed);
    }

    _format      = ChooseFormat(caps);
    _presentMode = ChoosePresentMode(caps, vsync);
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    return Resize(width, height);
}

// ─── Resize ───────────────────────────────────────────────────────────────────

VoidResult WebGPUSurface::Resize(std::uint32_t width, std::uint32_t height)
{
    if (!_surface || width == 0 || height == 0) return std::unexpected(Error::InvalidArgument);

    _width  = width;
    _height = height;

    WGPUSurfaceConfiguration config{};
    config.device      = _device;
    config.format       = _format;
    config.usage         = WGPUTextureUsage_RenderAttachment;
    config.width          = width;
    config.height         = height;
    config.alphaMode      = WGPUCompositeAlphaMode_Auto;
    config.presentMode    = _presentMode;
    config.viewFormatCount = 0;
    config.viewFormats     = nullptr;

    wgpuSurfaceConfigure(_surface, &config);
    return {};
}

// ─── AcquireCurrentTexture ────────────────────────────────────────────────────

WebGPUAcquireResult WebGPUSurface::AcquireCurrentTexture() const
{
    WebGPUAcquireResult result{};
    if (!_surface) return result;

    WGPUSurfaceTexture surfaceTexture{};
    wgpuSurfaceGetCurrentTexture(_surface, &surfaceTexture);

    switch (surfaceTexture.status) {
        case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
            result.texture = surfaceTexture.texture;
            result.ok      = true;
            break;
        case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
            // Still usable this frame — reconfigure before the next acquire.
            result.texture          = surfaceTexture.texture;
            result.ok               = true;
            result.needsReconfigure = true;
            break;
        case WGPUSurfaceGetCurrentTextureStatus_Timeout:
        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
            // No usable texture this frame, but recoverable next frame after a reconfigure.
            result.needsReconfigure = true;
            if (surfaceTexture.texture) wgpuTextureRelease(surfaceTexture.texture);
            break;
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
        case WGPUSurfaceGetCurrentTextureStatus_Error:
        default:
            // Lost/Error: device recreation is out of scope this phase — see proposal's Frame Cycle section.
            result.fatal = true;
            if (surfaceTexture.texture) wgpuTextureRelease(surfaceTexture.texture);
            break;
    }
    return result;
}

// ─── Present ──────────────────────────────────────────────────────────────────

void WebGPUSurface::Present() const
{
    if (_surface) wgpuSurfacePresent(_surface);
}

// ─── Destroy ──────────────────────────────────────────────────────────────────

void WebGPUSurface::Destroy()
{
    if (_surface) {
        wgpuSurfaceUnconfigure(_surface);
        wgpuSurfaceRelease(_surface);
        _surface = nullptr;
    }
    _instance = nullptr;
    _adapter  = nullptr;
    _device   = nullptr;
    _format   = WGPUTextureFormat_Undefined;
    _width    = 0;
    _height   = 0;
}

} // namespace ImFrame::Internal
