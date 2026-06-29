/**
 * @file     ViewportWebGPU.cpp
 * @brief    Dawn WebGPU viewport framebuffer — sequential-submit sync, no semaphore needed
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

#include <webgpu/webgpu.h>
#include <imgui_impl_wgpu.h>

#include "ViewportWebGPU.hpp"

namespace ImFrame::Internal {

ViewportFramebufferWebGPU::ViewportFramebufferWebGPU(
    WGPUDevice device, WGPUQueue queue,
    WGPUTextureFormat format,
    std::uint32_t width, std::uint32_t height)
    : _device(device)
    , _queue(queue)
    , _format(format) {
    Allocate(width, height);
}

ViewportFramebufferWebGPU::~ViewportFramebufferWebGPU() noexcept {
    if (_encoder) { wgpuCommandEncoderRelease(_encoder); _encoder = nullptr; }
    Release();
}

void ViewportFramebufferWebGPU::Resize(std::uint32_t width, std::uint32_t height) {
    Release();
    Allocate(width, height);
}

void ViewportFramebufferWebGPU::BeginRender(std::uint32_t /*frameIndex*/) {
    WGPUCommandEncoderDescriptor desc{};
    _encoder = wgpuDeviceCreateCommandEncoder(_device, &desc);
}

void ViewportFramebufferWebGPU::EndRender(std::uint32_t /*frameIndex*/) {
    if (!_encoder) return;
    WGPUCommandBufferDescriptor cbDesc{};
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(_encoder, &cbDesc);
    wgpuCommandEncoderRelease(_encoder);
    _encoder = nullptr;
    wgpuQueueSubmit(_queue, 1, &cmdBuf);
    wgpuCommandBufferRelease(cmdBuf);
    // WebGPU's sequential submit model guarantees the above work completes
    // before the next submit on this queue — no explicit fence needed.
}

ViewportHandles ViewportFramebufferWebGPU::GetHandles() const {
    ViewportHandles h;
    h.wgpuTex     = _texture;
    h.wgpuView    = _view;
    h.wgpuEncoder = _encoder;
    h.width       = _width;
    h.height      = _height;
    h.imTextureId = _imTextureId;
    return h;
}

void ViewportFramebufferWebGPU::Allocate(std::uint32_t width, std::uint32_t height) {
    _width  = width;
    _height = height;

    WGPUTextureDescriptor texDesc{};
    texDesc.usage         = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    texDesc.dimension     = WGPUTextureDimension_2D;
    texDesc.size          = { width, height, 1 };
    texDesc.format        = _format;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount   = 1;
    _texture = wgpuDeviceCreateTexture(_device, &texDesc);

    WGPUTextureViewDescriptor viewDesc{};
    viewDesc.format          = _format;
    viewDesc.dimension       = WGPUTextureViewDimension_2D;
    viewDesc.mipLevelCount   = 1;
    viewDesc.arrayLayerCount = 1;
    _view = wgpuTextureCreateView(_texture, &viewDesc);

    // imgui_impl_wgpu uses WGPUTextureView* directly as ImTextureID (cast to uint64).
    _imTextureId = static_cast<std::uint64_t>(reinterpret_cast<uintptr_t>(_view));
}

void ViewportFramebufferWebGPU::Release() noexcept {
    _imTextureId = 0;
    if (_view)    { wgpuTextureViewRelease(_view);  _view    = nullptr; }
    if (_texture) { wgpuTextureRelease(_texture);   _texture = nullptr; }
}

} // namespace ImFrame::Internal
