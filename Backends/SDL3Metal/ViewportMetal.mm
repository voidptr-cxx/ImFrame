/**
 * @file     ViewportMetal.mm
 * @brief    Metal IViewportFramebuffer method implementations
 *
 * Phase 24 sync: EndRender() enqueues a lightweight sync MTLCommandBuffer and
 * calls waitUntilCompleted — a CPU stall that ensures the user's render work
 * committed to MetalContext::CommandQueue has finished before ImGui samples
 * the texture. Replace with a MTLEvent-based approach for production use.
 *
 * UNVERIFIED — written on Windows, awaiting macOS build.
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

#ifdef __APPLE__

#import <Foundation/Foundation.h>
#include "ViewportMetal.hpp"

namespace ImFrame::Internal {

ViewportFramebufferMetal::ViewportFramebufferMetal(
    id<MTLDevice>       device,
    id<MTLCommandQueue> commandQueue,
    MTLPixelFormat      pixelFormat,
    std::uint32_t       width,
    std::uint32_t       height)
    : _device(device)
    , _commandQueue(commandQueue)
    , _pixelFormat(pixelFormat)
{
    Allocate(width, height);
}

ViewportFramebufferMetal::~ViewportFramebufferMetal() noexcept {
    Release();
}

void ViewportFramebufferMetal::Resize(std::uint32_t width, std::uint32_t height) {
    Release();
    Allocate(width, height);
}

void ViewportFramebufferMetal::BeginRender(std::uint32_t /*frameIndex*/) {
    // The user creates their own MTLCommandBuffer from MetalContext::CommandQueue.
}

void ViewportFramebufferMetal::EndRender(std::uint32_t /*frameIndex*/) {
    // Phase 24 stall: ensure the user's committed work has completed before
    // ImGui samples the texture. A MTLEvent-based approach is a Phase 24.x task.
    id<MTLCommandBuffer> syncBuf = [_commandQueue commandBuffer];
    [syncBuf commit];
    [syncBuf waitUntilCompleted];
}

ViewportHandles ViewportFramebufferMetal::GetHandles() const {
    ViewportHandles h;
    h.mtlTex      = (__bridge void*)_texture;
    h.width       = _width;
    h.height      = _height;
    h.imTextureId = _imTextureId;
    return h;
}

void ViewportFramebufferMetal::Allocate(std::uint32_t width, std::uint32_t height) {
    _width  = width;
    _height = height;

    MTLTextureDescriptor* desc = [MTLTextureDescriptor new];
    desc.textureType = MTLTextureType2D;
    desc.pixelFormat = _pixelFormat;
    desc.width       = width;
    desc.height      = height;
    desc.usage       = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModePrivate;
    _texture = [_device newTextureWithDescriptor:desc];

    // Use the raw bridged pointer as the ImTextureID for the Metal backend.
    // ImGui_ImplMetal_AddTexture is not universally available — fall back to
    // the bridged void* cast which imgui_impl_metal.mm supports internally.
    _imTextureId = reinterpret_cast<std::uint64_t>((__bridge void*)_texture);
}

void ViewportFramebufferMetal::Release() noexcept {
    _texture     = nil;
    _imTextureId = 0;
}

} // namespace ImFrame::Internal

#endif // __APPLE__
