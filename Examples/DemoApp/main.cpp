/**
 * @file     main.cpp
 * @brief    Phase 1 smoke test: opens a window, renders one ImGui frame, and exits
 *
 * @internal
 * This file exercises the full Init → Poll → BeginFrame → EndFrame → Shutdown
 * lifecycle of GLFWOpenGL3Backend. It is the CI end-to-end test for Phase 1.
 * Real demo content (panels, widgets, themes) is added from Phase 7 onwards.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 */

#include "ImFrame/ImFrame.hpp"
#include "GLFWOpenGL3Backend.hpp"

#include <cstdlib>  // EXIT_SUCCESS, EXIT_FAILURE

int main()
{
    ImFrame::Internal::GLFWOpenGL3Backend backend;

    ImFrame::WindowConfig config{
        .Title     = "ImFrame Smoke Test",
        .Width     = 800,
        .Height    = 600,
        .VSync     = false,  // disable VSync for headless CI speed
        .Docking   = false,
        .Viewports = false,
    };

    // ── Init ──────────────────────────────────────────────────────────────────
    auto result = backend.Init(config);
    if (!result) {
        return EXIT_FAILURE;
    }

    // ── One-frame smoke test ──────────────────────────────────────────────────
    if (!backend.Poll()) {
        backend.Shutdown();
        return EXIT_FAILURE;
    }

    backend.BeginFrame();
    // No UI code — blank frame verifies the render pipeline is functional.
    backend.EndFrame();

    // ── Shutdown ──────────────────────────────────────────────────────────────
    backend.Shutdown();
    return EXIT_SUCCESS;
}
