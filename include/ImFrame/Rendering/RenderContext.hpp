/**
 * @file     RenderContext.hpp
 * @brief    Per-frame render context and backend-specific framebuffer handles passed to Viewport callbacks
 *
 * Defines `ViewportImage` — a variant over six backend-specific framebuffer
 * handle structs — and `RenderContext`, the struct passed by const reference
 * to every Viewport `OnRender` callback. All GPU handles are `void*` to avoid
 * platform headers in the public API; casts are documented per-backend in the
 * struct member comments.
 *
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
#include "ImFrame/Widgets/Types.hpp"

#include <cstdint>
#include <variant>

namespace ImFrame::Rendering {

// ─── ViewportImage backend structs ───────────────────────────────────────────

/**
 * @struct ViewportImageGL
 * @brief  OpenGL framebuffer handles provided to the Viewport OnRender callback
 *
 * Bind `Framebuffer`, render into it using `ColorTexture` as the colour attachment,
 * then unbind. A depth/stencil renderbuffer is attached to `Framebuffer` for users
 * who need depth testing. `ColorTexture` is sampled by ImGui after the callback.
 *
 * @since 2.0.0
 */
struct ViewportImageGL {
    unsigned int Framebuffer;  ///< GLuint FBO. Bind before rendering, unbind after.
    unsigned int ColorTexture; ///< GLuint RGBA8 texture attached as GL_COLOR_ATTACHMENT0.
    std::uint32_t Width;       ///< Framebuffer width in pixels.
    std::uint32_t Height;      ///< Framebuffer height in pixels.
};

/**
 * @struct ViewportImageVulkan
 * @brief  Vulkan image and command buffer handles for the Viewport OnRender callback
 *
 * `Image` is in `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL` on callback entry.
 * Record render commands into `CommandBuffer`. After the callback returns, ImFrame
 * transitions the image to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` and submits.
 *
 * All handles are `void*` aliasing the true Vulkan dispatchable or non-dispatchable
 * handle types; cast with `reinterpret_cast` or `static_cast<VkFoo>`.
 *
 * @since 2.0.0
 */
struct ViewportImageVulkan {
    void*         Image;         ///< VkImage — layout: COLOR_ATTACHMENT_OPTIMAL on entry.
    void*         View;          ///< VkImageView for Image.
    void*         CommandBuffer; ///< VkCommandBuffer opened from VulkanContext::ViewportCommandPool.
    std::uint32_t ImageWidth;    ///< Framebuffer width in pixels.
    std::uint32_t ImageHeight;   ///< Framebuffer height in pixels.
};

/**
 * @struct ViewportImageMetal
 * @brief  Metal texture handle provided to the Viewport OnRender callback
 *
 * Create your own `MTLCommandBuffer` from `MetalContext::CommandQueue`, render
 * into `ColorTexture`, and commit. ImFrame synchronises the committed work with
 * the main encoder via a `MTLEvent` before the ImGui pass samples the texture.
 *
 * @since 2.0.0
 */
struct ViewportImageMetal {
    void*         ColorTexture; ///< MTLTexture* (MTLPixelFormatBGRA8Unorm_sRGB). ARC-managed.
    std::uint32_t Width;        ///< Texture width in pixels.
    std::uint32_t Height;       ///< Texture height in pixels.
};

/**
 * @struct ViewportImageDX12
 * @brief  Direct3D 12 resource and descriptor handles for the Viewport OnRender callback
 *
 * `RenderTarget` is in `D3D12_RESOURCE_STATE_RENDER_TARGET` on entry. Record
 * commands into `CommandList`. ImFrame closes, submits, and CPU-fences it before
 * the main render pass. `RTV` and `SRV` are the `.ptr` fields of the corresponding
 * descriptor handles.
 *
 * @since 2.0.0
 */
struct ViewportImageDX12 {
    void*          RenderTarget; ///< ID3D12Resource* in D3D12_RESOURCE_STATE_RENDER_TARGET.
    std::uintptr_t RTV;          ///< D3D12_CPU_DESCRIPTOR_HANDLE.ptr for the colour target.
    std::uint64_t  SRV;          ///< D3D12_GPU_DESCRIPTOR_HANDLE.ptr sampled by ImGui::Image().
    void*          CommandList;  ///< ID3D12GraphicsCommandList7* opened by ImFrame.
    std::uint32_t  Width;        ///< Render target width in pixels.
    std::uint32_t  Height;       ///< Render target height in pixels.
};

/**
 * @struct ViewportImageWebGPU
 * @brief  WebGPU texture and encoder handles for the Viewport OnRender callback
 *
 * Begin render passes on `Encoder`, record commands, and end them. Do NOT call
 * `wgpuCommandEncoderFinish()` — ImFrame calls it after the callback returns and
 * submits the resulting buffer before the main frame submit. WebGPU's sequential
 * queue model guarantees that the texture is ready when ImGui samples it.
 *
 * @since 2.0.0
 */
struct ViewportImageWebGPU {
    void*         Texture; ///< WGPUTexture (usage: RenderAttachment | TextureBinding).
    void*         View;    ///< WGPUTextureView for Texture.
    void*         Encoder; ///< WGPUCommandEncoder. Do not call wgpuCommandEncoderFinish().
    std::uint32_t Width;   ///< Texture width in pixels.
    std::uint32_t Height;  ///< Texture height in pixels.
};

/**
 * @struct ViewportImageHeadless
 * @brief  CPU pixel buffer provided to the Viewport OnRender callback in headless mode
 *
 * Write RGBA8 pixel data row-by-row into `Pixels`. The buffer is
 * `Width * Height * 4` bytes, owned by ImFrame, and valid only for the duration
 * of the callback. Read it back via `HeadlessViewport::ReadPixels()` after the
 * frame completes.
 *
 * @since 2.0.0
 */
struct ViewportImageHeadless {
    std::uint8_t* Pixels; ///< RGBA8 row-major buffer. Width * Height * 4 bytes. Not null.
    std::uint32_t Width;  ///< Buffer width in pixels.
    std::uint32_t Height; ///< Buffer height in pixels.
};

/**
 * @brief  Discriminated union over the six backend-specific framebuffer handle structs
 *
 * `DispatchViewportRenders()` fills the active backend's alternative before calling
 * `OnRender`. Use `std::get<ViewportImageXxx>()` or `std::visit()` to access the
 * handles relevant to your renderer.
 *
 * @since 2.0.0
 */
using ViewportImage = std::variant<
    ViewportImageGL,
    ViewportImageVulkan,
    ViewportImageMetal,
    ViewportImageDX12,
    ViewportImageWebGPU,
    ViewportImageHeadless>;

// ─── RenderContext ────────────────────────────────────────────────────────────

/**
 * @struct RenderContext
 * @brief  Per-frame context passed by const reference to the Viewport OnRender callback
 *
 * `NativeContext` provides the backend's GPU device handles (device, queue, etc.).
 * `NativeImage` is the destination framebuffer to render into this frame.
 *
 * The reference is valid only for the duration of the callback. Do not retain any
 * pointer to a member across frames — `NativeImage` handles may be recreated on
 * resize, and `NativeContext` contains borrowed pointers owned by the backend.
 *
 * @since 2.0.0
 *
 * @example
 * @code
 * Rendering::Viewport("scene")
 *     .OnRender([](const Rendering::RenderContext& ctx) {
 *         auto& gl = std::get<Rendering::ViewportImageGL>(ctx.NativeImage);
 *         glBindFramebuffer(GL_FRAMEBUFFER, gl.Framebuffer);
 *         glViewport(0, 0, (GLsizei)gl.Width, (GLsizei)gl.Height);
 *         // ... user render ...
 *         glBindFramebuffer(GL_FRAMEBUFFER, 0);
 *     })
 *     .Show();
 * @endcode
 */
struct RenderContext {
    Widgets::Vec2                Size;         ///< Viewport pixel dimensions (matches NativeImage Width/Height).
    float                        DeltaTime;    ///< Seconds since the previous frame.
    std::uint32_t                FrameIndex;   ///< Current frame-in-flight index in [0, framesInFlight).
    const NativeGraphicsContext& NativeContext; ///< Backend GPU context descriptor — device, queue, format.
    ViewportImage                NativeImage;   ///< Destination framebuffer. Valid only during this callback.
};

} // namespace ImFrame::Rendering
