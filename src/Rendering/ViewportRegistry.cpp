/**
 * @file     ViewportRegistry.cpp
 * @brief    Viewport registration, framebuffer lifetime, and per-frame dispatch
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Rendering/ViewportRegistry.hpp"

#include <algorithm>
#include <type_traits>

namespace ImFrame::Internal {

// ─── MakeViewportImage ────────────────────────────────────────────────────────

Rendering::ViewportImage MakeViewportImage(
    const NativeGraphicsContext& ctx, const ViewportHandles& h)
{
    return std::visit([&h](const auto& c) -> Rendering::ViewportImage {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, OpenGLContext>) {
            return Rendering::ViewportImageGL{
                h.glFBO, h.glColorTex, h.width, h.height };
        } else if constexpr (std::is_same_v<T, VulkanContext>) {
            return Rendering::ViewportImageVulkan{
                h.vkImage, h.vkView, h.vkCmdBuf, h.width, h.height };
        } else if constexpr (std::is_same_v<T, MetalContext>) {
            return Rendering::ViewportImageMetal{
                h.mtlTex, h.width, h.height };
        } else if constexpr (std::is_same_v<T, DX12Context>) {
            return Rendering::ViewportImageDX12{
                h.d3dResource, h.d3dRTV, h.d3dSRV, h.d3dCmdList, h.width, h.height };
        } else if constexpr (std::is_same_v<T, WebGPUContext>) {
            return Rendering::ViewportImageWebGPU{
                h.wgpuTex, h.wgpuView, h.wgpuEncoder, h.width, h.height };
        } else { // HeadlessContext
            return Rendering::ViewportImageHeadless{
                h.headlessPixels, h.width, h.height };
        }
    }, ctx);
}

// ─── ViewportRegistry ─────────────────────────────────────────────────────────

ViewportEntry& ViewportRegistry::GetOrCreate(
    Rendering::Viewport* vp,
    std::uint32_t requestedW, std::uint32_t requestedH,
    IBackend& backend)
{
    requestedW = std::max(requestedW, 1u);
    requestedH = std::max(requestedH, 1u);

    // Return existing entry, resizing the framebuffer if dimensions changed.
    for (auto& entry : _entries) {
        if (entry.id == vp->Id()) {
            entry.viewport = vp;
            if (entry.framebuffer &&
                (entry.currentW != requestedW || entry.currentH != requestedH)) {
                entry.framebuffer->Resize(requestedW, requestedH);
                entry.currentW    = requestedW;
                entry.currentH    = requestedH;
                entry.imTextureId = entry.framebuffer->GetHandles().imTextureId;
                vp->FireOnResize({static_cast<float>(requestedW),
                                  static_cast<float>(requestedH)});
            }
            return entry;
        }
    }

    // First appearance — allocate a new entry and framebuffer.
    auto& entry       = _entries.emplace_back();
    entry.id          = std::string(vp->Id());
    entry.viewport    = vp;
    entry.currentW    = requestedW;
    entry.currentH    = requestedH;
    entry.framebuffer = backend.CreateViewportFramebuffer(requestedW, requestedH);
    if (entry.framebuffer) {
        entry.imTextureId = entry.framebuffer->GetHandles().imTextureId;
    }
    vp->FireOnResize({static_cast<float>(requestedW), static_cast<float>(requestedH)});
    return entry;
}

void ViewportRegistry::Remove(std::string_view id) {
    auto it = std::find_if(_entries.begin(), _entries.end(),
        [id](const ViewportEntry& e) { return e.id == id; });
    if (it != _entries.end()) {
        _entries.erase(it);
    }
}

void ViewportRegistry::DispatchRenders(
    IBackend& backend, float deltaTime, std::uint32_t frameIndex)
{
    const NativeGraphicsContext ctx = backend.GetNativeGraphicsContext();
    for (auto& entry : _entries) {
        if (!entry.wasShown || !entry.viewport || !entry.framebuffer) continue;

        entry.framebuffer->BeginRender(frameIndex);
        const ViewportHandles handles = entry.framebuffer->GetHandles();

        const Rendering::RenderContext renderCtx{
            .Size          = { static_cast<float>(entry.currentW),
                               static_cast<float>(entry.currentH) },
            .DeltaTime     = deltaTime,
            .FrameIndex    = frameIndex,
            .NativeContext = ctx,
            .NativeImage   = MakeViewportImage(ctx, handles),
        };

        entry.viewport->FireOnRender(renderCtx);
        entry.framebuffer->EndRender(frameIndex);
        entry.viewport->CaptureAfterRender(handles);
    }
}

void ViewportRegistry::FlipShownFlags() {
    for (auto& entry : _entries) {
        entry.wasShown = entry.shownNow;
        entry.shownNow = false;
    }
}

void ViewportRegistry::MarkShown(std::string_view id, const ViewportScreenRect& rect) {
    for (auto& entry : _entries) {
        if (entry.id == id) {
            entry.shownNow   = true;
            entry.screenRect = rect;
            return;
        }
    }
}

void ViewportRegistry::DestroyAll() {
    _entries.clear();
}

} // namespace ImFrame::Internal
