/**
 * @file     DX12Input_test.cpp
 * @brief    Unit tests confirming SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED reuses TranslateWindowResize correctly
 *
 * The DX12 backend deliberately reuses Phase 20's `InputTranslation` module
 * unchanged (see Backends/SDL3DX12/CMakeLists.txt) and listens for
 * `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` instead of `SDL_EVENT_WINDOW_RESIZED`
 * (see the Phase 22 proposal's Input Handling section — DXGI's
 * `ResizeBuffers()` needs physical pixel dimensions under per-monitor DPI
 * awareness, which `PIXEL_SIZE_CHANGED` reports and `RESIZED` does not).
 *
 * `TranslateWindowResize()` itself is event-type-agnostic — it only reads
 * `SDL_WindowEvent::data1`/`data2` — so this file does not duplicate
 * VulkanInput_test.cpp's coverage of the function itself. It exists to lock
 * in the specific assumption the DX12 backend's `Poll()` depends on: that
 * `PIXEL_SIZE_CHANGED`'s event struct carries width/height in the same
 * `data1`/`data2` fields as `RESIZED`, so reusing the translator for it is
 * valid and not a silent type-shape coincidence.
 *
 * No SDL3 Init() or GPU is needed.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-19
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/SDL3Vulkan/InputTranslation.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Internal::InputTranslation;

TEST_CASE("TranslateWindowResize handles a PIXEL_SIZE_CHANGED-shaped event identically to RESIZED", "[unit]")
{
    SDL_WindowEvent e{};
    e.type  = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
    e.data1 = 2560;
    e.data2 = 1440;

    WindowResizeEvent wre = TranslateWindowResize(e, 7);
    REQUIRE(wre.Handle == 7);
    REQUIRE(wre.Width  == 2560);
    REQUIRE(wre.Height == 1440);
}

TEST_CASE("TranslateWindowResize result does not depend on the event's type field", "[unit]")
{
    SDL_WindowEvent resized{};
    resized.type  = SDL_EVENT_WINDOW_RESIZED;
    resized.data1 = 800;
    resized.data2 = 600;

    SDL_WindowEvent pixelChanged{};
    pixelChanged.type  = SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
    pixelChanged.data1 = 800;
    pixelChanged.data2 = 600;

    WindowResizeEvent a = TranslateWindowResize(resized, 1);
    WindowResizeEvent b = TranslateWindowResize(pixelChanged, 1);

    REQUIRE(a.Width  == b.Width);
    REQUIRE(a.Height == b.Height);
    REQUIRE(a.Handle == b.Handle);
}
