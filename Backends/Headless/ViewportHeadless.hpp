/**
 * @file     ViewportHeadless.hpp
 * @brief    Headless-backend IViewportFramebuffer — CPU RGBA8 pixel buffer
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

#include "ImFrame/Backends/BackendInfo.hpp"

#include <cstdint>
#include <vector>

namespace ImFrame::Internal {

/// CPU pixel buffer framebuffer for the HeadlessBackend.
class ViewportFramebufferHeadless final : public IViewportFramebuffer {
public:
    ViewportFramebufferHeadless(std::uint32_t width, std::uint32_t height);

    void Resize(std::uint32_t width, std::uint32_t height) override;
    void BeginRender(std::uint32_t frameIndex) override;
    void EndRender(std::uint32_t frameIndex) override;
    [[nodiscard]] ViewportHandles GetHandles() const override;

private:
    std::uint32_t              _width  = 0;
    std::uint32_t              _height = 0;
    std::vector<std::uint8_t>  _pixels; ///< RGBA8, width*height*4 bytes.
};

} // namespace ImFrame::Internal
