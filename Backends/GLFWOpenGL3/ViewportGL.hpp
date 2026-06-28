/**
 * @file     ViewportGL.hpp
 * @brief    OpenGL IViewportFramebuffer — FBO + RGBA8 texture + depth renderbuffer
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

#include <cstdint>

namespace ImFrame::Internal {

/// OpenGL FBO-backed framebuffer for Phase 24 Viewport.
class ViewportFramebufferGL final : public IViewportFramebuffer {
public:
    ViewportFramebufferGL(std::uint32_t width, std::uint32_t height);
    ~ViewportFramebufferGL() noexcept override;

    void Resize(std::uint32_t width, std::uint32_t height) override;
    void BeginRender(std::uint32_t frameIndex) override;
    void EndRender(std::uint32_t frameIndex) override;
    [[nodiscard]] ViewportHandles GetHandles() const override;

private:
    void Allocate(std::uint32_t width, std::uint32_t height);
    void Release() noexcept;

    unsigned int  _fbo       = 0; ///< GLuint FBO.
    unsigned int  _colorTex  = 0; ///< GLuint RGBA8 colour texture.
    unsigned int  _depthRbo  = 0; ///< GLuint DEPTH24_STENCIL8 renderbuffer.
    std::uint32_t _width     = 0;
    std::uint32_t _height    = 0;
    std::uint64_t _texId     = 0; ///< ImTextureID cast from colorTex.
};

} // namespace ImFrame::Internal
