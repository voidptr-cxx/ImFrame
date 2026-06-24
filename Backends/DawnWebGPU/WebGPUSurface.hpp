/**
 * @file     WebGPUSurface.hpp
 * @brief    WGPUSurface lifetime wrapper — configure, resize, acquire, present
 *
 * WebGPU's surface model is the simplest of the four real-hardware backends:
 * `wgpuSurfaceConfigure()` owns the back-buffer count and format internally —
 * there is no explicit back-buffer array to manage (unlike `DX12SwapChain`'s
 * RTV array or Vulkan's `SwapChain` image vector). Resize is a reconfigure,
 * never a recreation.
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

#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Core/Error.hpp"

#include <cstdint>
#include <webgpu/webgpu.h>

namespace ImFrame::Internal {

/**
 * @struct  WebGPUAcquireResult
 * @brief   Outcome of `WebGPUSurface::AcquireCurrentTexture()`
 * @since   2.3.0
 */
struct WebGPUAcquireResult {
    WGPUTexture texture       = nullptr; ///< Owning reference — caller must wgpuTextureRelease() it.
    bool        ok            = false;   ///< True if `texture` is usable this frame.
    bool        needsReconfigure = false; ///< True if the surface should be reconfigured before the next acquire.
    bool        fatal         = false;   ///< True on WGPUSurfaceGetCurrentTextureStatus_Error/Lost — non-recoverable this phase.
};

/**
 * @class    WebGPUSurface
 * @brief    Owns one `WGPUSurface` and its current configuration
 *
 * @since    2.3.0
 */
class WebGPUSurface {
public:
    WebGPUSurface()  = default;
    ~WebGPUSurface() { Destroy(); }

    WebGPUSurface(const WebGPUSurface&)            = delete;
    WebGPUSurface& operator=(const WebGPUSurface&) = delete;
    WebGPUSurface(WebGPUSurface&& other) noexcept;
    WebGPUSurface& operator=(WebGPUSurface&& other) noexcept;

    /**
     * @brief    Take ownership of an already-created `WGPUSurface` and configure it.
     *
     * @param[in]  surface  Surface created by the platform-specific helper. Takes ownership.
     * @param[in]  instance Owning instance (used to query capabilities alongside `adapter`).
     * @param[in]  adapter  Adapter used to query `wgpuSurfaceGetCapabilities()`.
     * @param[in]  device   Device the surface will render with.
     * @param[in]  width    Initial drawable width in pixels.
     * @param[in]  height   Initial drawable height in pixels.
     * @param[in]  vsync    Requested VSync mode (falls back per Phase 22 precedent).
     * @return   Empty result on success; an `Error` on failure.
     */
    VoidResult Configure(WGPUSurface surface, WGPUInstance instance, WGPUAdapter adapter, WGPUDevice device,
                         std::uint32_t width, std::uint32_t height, VSyncMode vsync);

    /**
     * @brief    Reconfigure for a new drawable size. Never recreates the surface.
     */
    VoidResult Resize(std::uint32_t width, std::uint32_t height);

    /**
     * @brief    Acquire this frame's texture. See `WebGPUAcquireResult` for status handling.
     */
    [[nodiscard]] WebGPUAcquireResult AcquireCurrentTexture() const;

    /**
     * @brief    Present the frame previously acquired via `AcquireCurrentTexture()`.
     */
    void Present() const;

    /**
     * @brief    Release the surface and all cached state. Safe to call multiple times.
     */
    void Destroy();

    [[nodiscard]] WGPUTextureFormat Format() const noexcept { return _format; }
    [[nodiscard]] bool              IsValid() const noexcept { return _surface != nullptr; }

private:
    WGPUSurface       _surface  = nullptr;
    WGPUInstance       _instance = nullptr; ///< Non-owning — borrowed from the backend for capability queries.
    WGPUAdapter        _adapter  = nullptr; ///< Non-owning.
    WGPUDevice         _device   = nullptr; ///< Non-owning.
    WGPUTextureFormat  _format   = WGPUTextureFormat_Undefined;
    WGPUPresentMode    _presentMode = WGPUPresentMode_Fifo;
    std::uint32_t      _width    = 0;
    std::uint32_t      _height   = 0;
};

} // namespace ImFrame::Internal
