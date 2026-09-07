/**
 * @file     SurfaceEmscripten.hpp
 * @brief    Create a WGPUSurface from an HTML canvas element (Emscripten only)
 *
 * @internal
 * UNVERIFIED — written from the verified `emdawnwebgpu` package headers
 * (`webgpu.h` downloaded from the `google/dawn` release matching this
 * project's pinned Dawn version) but never compiled; no Emscripten-target
 * build of ImFrame has been attempted. See PHASE_STATUS.md and DECISIONS.md.
 *
 * `ImGui_ImplWGPU_CreateWGPUSurfaceHelper()` (used by `NativeSurface.cpp` for
 * Windows/macOS/Linux) explicitly excludes Emscripten — its declaration in
 * `imgui_impl_wgpu.h` is guarded by `#ifndef __EMSCRIPTEN__`. The canvas
 * surface is built directly here instead, via
 * `WGPUEmscriptenSurfaceSourceCanvasHTMLSelector` — note this is *not* the
 * `WGPUSurfaceDescriptorFromCanvasHTMLSelector` name the Phase 23 proposal
 * assumed; see DECISIONS.md for the verified rename.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <string_view>
#include <webgpu/webgpu.h>

namespace ImFrame::Internal {

/**
 * @brief   Create a `WGPUSurface` targeting an HTML canvas element.
 *
 * @param[in]  instance  Owning `WGPUInstance`.
 * @param[in]  selector  CSS selector of the canvas (e.g. `"#canvas"`).
 * @return   A new `WGPUSurface`, or `nullptr` on failure.
 */
[[nodiscard]] WGPUSurface CreateEmscriptenSurface(WGPUInstance instance, std::string_view selector);

} // namespace ImFrame::Internal
