/**
 * @file     ViewportWebGPU.hpp
 * @brief    Dawn WebGPU IViewportFramebuffer — WGPUTexture + WGPUCommandEncoder
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

#include <webgpu/webgpu.h>

#include <cstdint>

namespace ImFrame::Internal {

/**
 * WebGPU offscreen framebuffer for Phase 24 Viewport.
 *
 * WebGPU's sequential submit model on a single queue guarantees that the
 * viewport encoder (finished + submitted in EndRender) completes before the
 * main frame encoder (submitted in BeginFrame) samples the texture. No
 * explicit semaphore is needed.
 */
class ViewportFramebufferWebGPU final : public IViewportFramebuffer {
public:
    ViewportFramebufferWebGPU(WGPUDevice device,
                              WGPUQueue  queue,
                              WGPUTextureFormat format,
                              std::uint32_t width,
                              std::uint32_t height);
    ~ViewportFramebufferWebGPU() noexcept override;

    void Resize(std::uint32_t width, std::uint32_t height) override;
    void BeginRender(std::uint32_t frameIndex) override;
    void EndRender(std::uint32_t frameIndex) override;
    [[nodiscard]] ViewportHandles GetHandles() const override;

private:
    void Allocate(std::uint32_t width, std::uint32_t height);
    void Release() noexcept;

    WGPUDevice         _device  = nullptr;
    WGPUQueue          _queue   = nullptr;
    WGPUTextureFormat  _format  = WGPUTextureFormat_BGRA8Unorm;
    WGPUTexture        _texture = nullptr;
    WGPUTextureView    _view    = nullptr;
    WGPUCommandEncoder _encoder = nullptr; ///< Created in BeginRender, finished in EndRender.
    std::uint64_t      _imTextureId = 0;
    std::uint32_t      _width  = 0;
    std::uint32_t      _height = 0;
};

} // namespace ImFrame::Internal
