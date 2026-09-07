/**
 * @file     MetalLayer.hpp
 * @brief    Per-window Metal presentation surface — wraps a CAMetalLayer or an offscreen texture
 *
 * `MetalLayer` mirrors the Phase 20 `SwapChain` struct in responsibility but is
 * significantly simpler — Metal manages drawable pooling internally, so there
 * is no explicit image-count negotiation or manual recreation on resize.
 *
 * Two modes:
 * - **Windowed**: wraps the `CAMetalLayer` SDL3 already attached to the
 *   window's `NSView` (obtained via `SDL_Metal_GetLayer()`). ImFrame configures
 *   the layer's pixel format, drawable count, and sync mode but does not own
 *   or create it.
 * - **Headless**: skips `CAMetalLayer` entirely and renders into an offscreen
 *   `MTLTexture` instead, used by `Application::CreateHeadless()`.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-18
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Core/Error.hpp"

#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>

namespace ImFrame::Internal {

/**
 * @struct MetalLayerDesc
 * @brief  Parameters for `MetalLayer::Create()`
 * @since  2.1.0
 */
struct MetalLayerDesc {
    id<MTLDevice> device = nil;
    /// CAMetalLayer obtained via `SDL_Metal_GetLayer()`. Ignored when `headless` is true.
    CAMetalLayer* layer = nil;
    VSyncMode     vsyncMode      = VSyncMode::On;
    bool          hdrOutput      = false;
    int           framesInFlight = 2;
    int           drawableWidth  = 0;
    int           drawableHeight = 0;
    /// True creates an offscreen `MTLTexture` render target instead of configuring `layer`.
    bool          headless = false;
};

/**
 * @struct MetalLayer
 * @brief  Owns the per-window drawable acquisition state and render pass descriptor
 *
 * Lifetime mirrors the window. Call `Create()` once after the SDL3 window (and,
 * for windowed mode, its `CAMetalLayer`) exists. `Resize()` updates
 * `drawableSize` only — Metal needs no further recreation on resize, unlike
 * Vulkan's swap chain.
 *
 * @since 2.1.0
 */
struct MetalLayer {
    /// Not owned — SDL3 owns the underlying `NSView`/`CAMetalLayer`. nil when headless.
    CAMetalLayer*            layer                 = nil;
    id<CAMetalDrawable>      currentDrawable        = nil; ///< Acquired by `Acquire()`; valid for one frame.
    MTLRenderPassDescriptor* renderPassDescriptor   = nil; ///< Reused every frame; colour attachment texture swapped per frame.
    id<MTLTexture>           headlessTexture         = nil; ///< Populated only when `headless == true`.
    MTLPixelFormat           pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    int                      drawableWidth  = 0;
    int                      drawableHeight = 0;
    bool                     headless = false;

    /**
     * @brief   Configure the layer (windowed) or allocate the offscreen texture (headless).
     *
     * @param[in]  desc  Creation parameters.
     * @return  Empty result on success; an `Error` on failure.
     * @throws  Nothing.
     */
    VoidResult Create(const MetalLayerDesc& desc);

    /**
     * @brief   Release the offscreen texture. Safe to call on a default-constructed instance.
     *
     * The `CAMetalLayer` itself is never destroyed here — SDL3 owns it.
     */
    void Destroy();

    /**
     * @brief   Acquire the next drawable (windowed) or confirm the offscreen texture is ready (headless).
     *
     * `[CAMetalLayer nextDrawable]` can return `nil` under memory pressure or
     * when the window is minimized/occluded — callers must skip the frame
     * without submitting GPU work when this returns `false`.
     *
     * @return  `true` if a drawable/texture is ready to render into this frame.
     */
    bool Acquire();

    /**
     * @brief   Point the render pass descriptor's colour attachment at the
     *          just-acquired drawable's texture (or the offscreen texture).
     *
     * @param[in]  loadAction   `MTLLoadActionClear` for a new frame.
     * @param[in]  clearColor   Clear colour, derived from the active theme.
     */
    void UpdateRenderPassAttachment(MTLLoadAction loadAction, MTLClearColor clearColor);

    /**
     * @brief   Encode `presentDrawable:` into the command buffer.
     *
     * Must be called **before** `[commandBuffer commit]` — Metal requires the
     * present call to be encoded into the buffer, unlike Vulkan's separate
     * present-queue submission. No-op when headless (nothing to present).
     *
     * @param[in]  commandBuffer  The command buffer about to be committed.
     */
    void Present(id<MTLCommandBuffer> commandBuffer);

    /**
     * @brief   Update `drawableSize` after a `WindowResizeEvent`.
     *
     * No other resource is recreated — Metal handles resize at the drawable
     * level automatically. Calling anything beyond this in response to resize
     * is a bug (see Phase 21 proposal's "Resize requires no resource
     * recreation" invariant).
     *
     * @param[in]  width   New drawable width in pixels.
     * @param[in]  height  New drawable height in pixels.
     */
    void Resize(int width, int height);
};

} // namespace ImFrame::Internal
