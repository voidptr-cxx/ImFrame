/**
 * @file     ViewportGL.cpp
 * @brief    OpenGL viewport framebuffer — FBO + RGBA8 colour texture + depth renderbuffer
 *
 * Uses DSA (Direct State Access) GL 4.5 functions (glCreateTextures,
 * glCreateFramebuffers, glNamedFramebufferTexture) matching the GLAD profile
 * already required by the GLFW+OpenGL3 backend.
 *
 * No synchronisation is needed: GL is single-threaded and the viewport render
 * completes on the same context before BeginFrame() starts.
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

#include <glad/glad.h>
// GLFW must come after GLAD.
#include <GLFW/glfw3.h>

#include "Backends/GLFWOpenGL3/ViewportGL.hpp"

namespace ImFrame::Internal {

ViewportFramebufferGL::ViewportFramebufferGL(std::uint32_t width, std::uint32_t height) {
    Allocate(width, height);
}

ViewportFramebufferGL::~ViewportFramebufferGL() noexcept {
    Release();
}

void ViewportFramebufferGL::Resize(std::uint32_t width, std::uint32_t height) {
    Release();
    Allocate(width, height);
}

void ViewportFramebufferGL::BeginRender(std::uint32_t /*frameIndex*/) {
    // Single-threaded GL — nothing to transition.
}

void ViewportFramebufferGL::EndRender(std::uint32_t /*frameIndex*/) {
    // Single-threaded GL — texture is ready for ImGui sampling without explicit sync.
}

ViewportHandles ViewportFramebufferGL::GetHandles() const {
    ViewportHandles h;
    h.glFBO      = _fbo;
    h.glColorTex = _colorTex;
    h.width      = _width;
    h.height     = _height;
    h.imTextureId = _texId;
    return h;
}

void ViewportFramebufferGL::Allocate(std::uint32_t width, std::uint32_t height) {
    _width  = width;
    _height = height;

    // Colour texture (RGBA8).
    glCreateTextures(GL_TEXTURE_2D, 1, &_colorTex);
    glTextureStorage2D(_colorTex, 1, GL_RGBA8,
                       static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    glTextureParameteri(_colorTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(_colorTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(_colorTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(_colorTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Depth+stencil renderbuffer.
    glCreateRenderbuffers(1, &_depthRbo);
    glNamedRenderbufferStorage(_depthRbo, GL_DEPTH24_STENCIL8,
                               static_cast<GLsizei>(width), static_cast<GLsizei>(height));

    // FBO.
    glCreateFramebuffers(1, &_fbo);
    glNamedFramebufferTexture(_fbo, GL_COLOR_ATTACHMENT0, _colorTex, 0);
    glNamedFramebufferRenderbuffer(_fbo, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_RENDERBUFFER, _depthRbo);

    // ImTextureID is the raw GLuint cast to uint64_t.
    _texId = static_cast<std::uint64_t>(_colorTex);
}

void ViewportFramebufferGL::Release() noexcept {
    if (_fbo)      { glDeleteFramebuffers(1, &_fbo);      _fbo      = 0; }
    if (_colorTex) { glDeleteTextures(1, &_colorTex);     _colorTex = 0; }
    if (_depthRbo) { glDeleteRenderbuffers(1, &_depthRbo); _depthRbo = 0; }
    _texId = 0;
}

} // namespace ImFrame::Internal
