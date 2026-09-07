/**
 * @file     WebGPUInput_test.cpp
 * @brief    Unit tests confirming SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED reuses TranslateWindowResize correctly
 *
 * Mirrors `DX12Input_test.cpp`'s rationale: `DawnWebGPUBackend` reuses Phase
 * 20's `InputTranslation` module unchanged and listens for
 * `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` rather than `SDL_EVENT_WINDOW_RESIZED`,
 * since `wgpuSurfaceConfigure()` needs physical pixel dimensions under
 * per-monitor DPI awareness. No SDL3 Init() or GPU is needed.
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

#include "Backends/SDL3Vulkan/InputTranslation.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Internal::InputTranslation;

TEST_CASE("TranslateWindowResize handles a PIXEL_SIZE_CHANGED-shaped event for the WebGPU backend", "[unit]")
{
    SDL_WindowEvent e{};
    e.type  = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
    e.data1 = 1920;
    e.data2 = 1080;

    WindowResizeEvent wre = TranslateWindowResize(e, 3);
    REQUIRE(wre.Handle == 3);
    REQUIRE(wre.Width  == 1920);
    REQUIRE(wre.Height == 1080);
}
