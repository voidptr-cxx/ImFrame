/**
 * @file     SurfaceEmscripten.cpp
 * @brief    CreateEmscriptenSurface() — canvas selector -> WGPUSurface via WGPUEmscriptenSurfaceSourceCanvasHTMLSelector
 *
 * @internal
 * UNVERIFIED — see SurfaceEmscripten.hpp.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "SurfaceEmscripten.hpp"

namespace ImFrame::Internal {

WGPUSurface CreateEmscriptenSurface(WGPUInstance instance, std::string_view selector)
{
    if (!instance || selector.empty()) return nullptr;

    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasSource{};
    canvasSource.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvasSource.selector    = { selector.data(), selector.size() };

    WGPUSurfaceDescriptor surfaceDesc{};
    surfaceDesc.nextInChain = &canvasSource.chain;

    return wgpuInstanceCreateSurface(instance, &surfaceDesc);
}

} // namespace ImFrame::Internal
