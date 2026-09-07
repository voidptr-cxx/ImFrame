/**
 * @file     ViewportMetal.hpp
 * @brief    Metal IViewportFramebuffer — MTLTexture offscreen target (Objective-C++ header)
 *
 * Include only from .mm translation units.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#ifdef __APPLE__

#import <Metal/Metal.h>
#include "ImFrame/Backends/BackendInfo.hpp"
#include <cstdint>

namespace ImFrame::Internal {

/**
 * Metal offscreen framebuffer for Phase 24 Viewport.
 *
 * Creates a BGRA8Unorm_sRGB MTLTexture with RenderTarget|ShaderRead usage.
 * The user creates their own MTLCommandBuffer from MetalContext::CommandQueue,
 * renders into ColorTexture, and commits it. ImFrame enqueues a sync
 * MTLCommandBuffer in EndRender() as a Phase 24 CPU stall to ensure the
 * texture is ready before ImGui samples it.
 *
 * UNVERIFIED — written on Windows, awaiting macOS build.
 */
class ViewportFramebufferMetal final : public IViewportFramebuffer {
public:
    ViewportFramebufferMetal(id<MTLDevice>       device,
                             id<MTLCommandQueue> commandQueue,
                             MTLPixelFormat      pixelFormat,
                             std::uint32_t       width,
                             std::uint32_t       height);
    ~ViewportFramebufferMetal() noexcept override;

    void Resize(std::uint32_t width, std::uint32_t height) override;
    void BeginRender(std::uint32_t frameIndex) override;
    void EndRender(std::uint32_t frameIndex) override;
    [[nodiscard]] ViewportHandles GetHandles() const override;

private:
    void Allocate(std::uint32_t width, std::uint32_t height);
    void Release() noexcept;

    id<MTLDevice>       _device       = nil;
    id<MTLCommandQueue> _commandQueue = nil;
    MTLPixelFormat      _pixelFormat  = MTLPixelFormatBGRA8Unorm_sRGB;
    id<MTLTexture>      _texture      = nil;
    std::uint32_t       _width        = 0;
    std::uint32_t       _height       = 0;
    std::uint64_t       _imTextureId  = 0;
};

} // namespace ImFrame::Internal

#endif // __APPLE__
