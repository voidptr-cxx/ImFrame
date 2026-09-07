/**
 * @file     ViewportHeadless.cpp
 * @brief    Headless viewport framebuffer — CPU RGBA8 pixel buffer implementation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ViewportHeadless.hpp"

namespace ImFrame::Internal {

ViewportFramebufferHeadless::ViewportFramebufferHeadless(
    std::uint32_t width, std::uint32_t height) {
    Resize(width, height);
}

void ViewportFramebufferHeadless::Resize(std::uint32_t width, std::uint32_t height) {
    _width  = width;
    _height = height;
    _pixels.assign(static_cast<std::size_t>(width) * height * 4, 0);
}

void ViewportFramebufferHeadless::BeginRender(std::uint32_t /*frameIndex*/) {
    // No transition needed — CPU buffer is always writable.
}

void ViewportFramebufferHeadless::EndRender(std::uint32_t /*frameIndex*/) {
    // No submit needed — caller reads _pixels directly via GetHandles().
}

ViewportHandles ViewportFramebufferHeadless::GetHandles() const {
    ViewportHandles h;
    h.headlessPixels = _pixels.empty() ? nullptr : const_cast<std::uint8_t*>(_pixels.data());
    h.width          = _width;
    h.height         = _height;
    // Sentinel 1 matches the headless font atlas TexID set in HeadlessBackend::Init.
    // A non-zero value lets Viewport::Show() call ImGui::Image() and MarkShown().
    h.imTextureId    = 1;
    return h;
}

} // namespace ImFrame::Internal
