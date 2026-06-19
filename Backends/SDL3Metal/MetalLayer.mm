/**
 * @file     MetalLayer.mm
 * @brief    CAMetalLayer configuration, drawable acquisition, present, and resize
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-18
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "MetalLayer.hpp"

#import <AppKit/AppKit.h>

namespace ImFrame::Internal {

// ─── Create ───────────────────────────────────────────────────────────────────

VoidResult MetalLayer::Create(const MetalLayerDesc& desc)
{
    drawableWidth  = desc.drawableWidth;
    drawableHeight = desc.drawableHeight;
    headless       = desc.headless;
    pixelFormat    = MTLPixelFormatBGRA8Unorm_sRGB;

    if (desc.headless) {
        MTLTextureDescriptor* texDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                                                                 width:static_cast<NSUInteger>(desc.drawableWidth)
                                                                height:static_cast<NSUInteger>(desc.drawableHeight)
                                                             mipmapped:NO];
        texDesc.usage       = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        texDesc.storageMode = MTLStorageModeShared;

        headlessTexture = [desc.device newTextureWithDescriptor:texDesc];
        if (!headlessTexture) return std::unexpected(Error::GraphicsInitFailed);
        // Always valid so BeginFrame() can call ImGui_ImplMetal_NewFrame() even
        // on a frame that ends up being skipped (e.g. zero-area minimized window).
        renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
        return {};
    }

    if (!desc.layer) return std::unexpected(Error::GraphicsInitFailed);
    layer = desc.layer;

    // HDR availability is checked, not assumed — falls back to SDR silently
    // on monitors that don't support extended dynamic range.
    bool useHdr = false;
    if (desc.hdrOutput) {
        NSScreen* mainScreen = [NSScreen mainScreen];
        if (mainScreen && mainScreen.maximumPotentialExtendedDynamicRangeColorComponentValue > 1.0) {
            useHdr = true;
        }
    }

    if (useHdr) {
        pixelFormat = MTLPixelFormatRGBA16Float;
        layer.wantsExtendedDynamicRangeContent = YES;
        // CGColorSpaceRef is a Core Foundation type, not ARC-managed — the
        // matching CFRelease below follows CF's create-rule ownership
        // convention. This is unrelated to the Phase 21 "no manual
        // retain/release" invariant, which applies to Objective-C objects only.
        CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
        layer.colorspace = colorSpace;
        CGColorSpaceRelease(colorSpace);
    } else {
        layer.wantsExtendedDynamicRangeContent = NO;
    }

    layer.device               = desc.device;
    layer.pixelFormat          = pixelFormat;
    layer.framebufferOnly      = NO; // allows ReadPixels() via blit
    layer.maximumDrawableCount = static_cast<NSUInteger>(desc.framesInFlight);
    layer.displaySyncEnabled   = (desc.vsyncMode != VSyncMode::Off);
    layer.drawableSize         = CGSizeMake(desc.drawableWidth, desc.drawableHeight);

    renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];

    return {};
}

// ─── Destroy ──────────────────────────────────────────────────────────────────

void MetalLayer::Destroy()
{
    headlessTexture       = nil;
    currentDrawable        = nil;
    renderPassDescriptor   = nil;
    // `layer` is never released here — SDL3 owns the CAMetalLayer for the
    // lifetime of its window.
    layer = nil;
}

// ─── Acquire ──────────────────────────────────────────────────────────────────

bool MetalLayer::Acquire()
{
    if (headless) return headlessTexture != nil;
    if (!layer) return false;

    currentDrawable = [layer nextDrawable];
    return currentDrawable != nil;
}

// ─── UpdateRenderPassAttachment ───────────────────────────────────────────────

void MetalLayer::UpdateRenderPassAttachment(MTLLoadAction loadAction, MTLClearColor clearColor)
{
    if (!renderPassDescriptor) {
        renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    }

    id<MTLTexture> target = headless ? headlessTexture : (currentDrawable ? currentDrawable.texture : nil);

    renderPassDescriptor.colorAttachments[0].texture     = target;
    renderPassDescriptor.colorAttachments[0].loadAction  = loadAction;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    renderPassDescriptor.colorAttachments[0].clearColor  = clearColor;
}

// ─── Present ──────────────────────────────────────────────────────────────────

void MetalLayer::Present(id<MTLCommandBuffer> commandBuffer)
{
    if (headless || !currentDrawable) return;
    [commandBuffer presentDrawable:currentDrawable];
}

// ─── Resize ───────────────────────────────────────────────────────────────────

void MetalLayer::Resize(int width, int height)
{
    drawableWidth  = width;
    drawableHeight = height;

    // Headless textures are not reallocated on resize — Application's
    // headless windows are not resized in current usage. Windowed mode needs
    // no resource recreation at all (Metal's ergonomic advantage over Vulkan).
    if (!headless && layer) {
        layer.drawableSize = CGSizeMake(width, height);
    }
}

} // namespace ImFrame::Internal
