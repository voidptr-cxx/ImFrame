/**
 * @file     NativeSurface.hpp
 * @brief    Create a WGPUSurface for an SDL3 window on any native platform
 *
 * @internal
 * One cross-platform translation unit, not four. `imgui_impl_wgpu.h`'s
 * `ImGui_ImplWGPU_CreateWGPUSurfaceHelper()` already contains the
 * platform-specific `WGPUSurfaceDescriptor` construction for Windows/Cocoa/
 * X11/Wayland (verified against imgui 1.92.8's actual source — see
 * DECISIONS.md). SDL3's `SDL_PROP_WINDOW_*_POINTER` property-name macros are
 * unconditional `#define`s present on every platform's `<SDL3/SDL_video.h>`
 * regardless of build target (verified against SDL's source), so a single
 * runtime branch on `SDL_GetCurrentVideoDriver()` is sufficient — no
 * per-platform compiled `.cpp`/`.mm` files and no Objective-C++ are needed
 * for surface creation.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <SDL3/SDL.h>
#include <webgpu/webgpu.h>

namespace ImFrame::Internal {

/**
 * @brief   Create a `WGPUSurface` targeting the given SDL3 window.
 *
 * @param[in]  instance   Owning `WGPUInstance`.
 * @param[in]  sdlWindow  SDL3 window to create the surface for.
 * @return   A new `WGPUSurface`, or `nullptr` if the platform/driver combination is unsupported.
 */
[[nodiscard]] WGPUSurface CreateNativeSurface(WGPUInstance instance, SDL_Window* sdlWindow);

} // namespace ImFrame::Internal
