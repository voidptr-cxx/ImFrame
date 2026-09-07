/**
 * @file     NativeSurface.cpp
 * @brief    CreateNativeSurface() — SDL3 property lookup + ImGui_ImplWGPU_CreateWGPUSurfaceHelper()
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "NativeSurface.hpp"

#include <imgui_impl_wgpu.h>

#include <cstdint>
#include <SDL3/SDL_properties.h>
#include <string>

namespace ImFrame::Internal {

WGPUSurface CreateNativeSurface(WGPUInstance instance, SDL_Window* sdlWindow)
{
    if (!instance || !sdlWindow) return nullptr;

    SDL_PropertiesID props = SDL_GetWindowProperties(sdlWindow);
    std::string      driver = SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "";

    ImGui_ImplWGPU_CreateSurfaceInfo info{};
    info.Instance = instance;

    if (driver == "windows") {
        info.System      = "win32";
        info.RawWindow   = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        info.RawInstance = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
        if (!info.RawWindow) return nullptr;
    } else if (driver == "cocoa") {
        info.System    = "cocoa";
        info.RawWindow = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
        if (!info.RawWindow) return nullptr;
    } else if (driver == "wayland") {
        info.System     = "wayland";
        info.RawDisplay = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
        info.RawSurface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        if (!info.RawDisplay || !info.RawSurface) return nullptr;
    } else if (driver == "x11") {
        info.System     = "x11";
        info.RawDisplay = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        // X11 Window is an XID (unsigned long), exposed by SDL3 as a number
        // property, not a pointer property — cast through uintptr_t to match
        // ImGui_ImplWGPU_CreateSurfaceInfo::RawWindow's void* slot.
        auto xid = static_cast<std::uintptr_t>(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
        info.RawWindow = reinterpret_cast<void*>(xid);
        if (!info.RawDisplay || xid == 0) return nullptr;
    } else {
        return nullptr; // Unsupported video driver for WebGPU surface creation.
    }

    return ImGui_ImplWGPU_CreateWGPUSurfaceHelper(&info);
}

} // namespace ImFrame::Internal
