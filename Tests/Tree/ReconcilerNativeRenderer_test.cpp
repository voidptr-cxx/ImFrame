/**
 * @file     ReconcilerNativeRenderer_test.cpp
 * @brief    Proves NativeRendererGL3 draws correctly from within a real Reconciler::Show() call (Phase 32.5)
 *
 * @internal
 * Drives `Internal::Reconciler::Show()` — the actual production call site, via
 * `RootBridge::BeginRootWindow()`, exactly as `Application::Run()`'s frame
 * tick does — with `Internal::NativeRendererGL3` swapped in via
 * `SetRenderer()`, through a real `GLFWOpenGL3Backend`. Confirms the
 * `Position`/viewport coordinate math, the real immediate GL draw call, and
 * this sub-phase's clear-timing fix (`glClear()` moved from `EndFrame()` to
 * `BeginFrame()` — see `.claude/DECISIONS.md`, Phase 32.5) are all correct,
 * by reading the framebuffer's pixels **immediately after `Show()` returns**
 * via a direct `glReadPixels()` call — not through
 * `GLFWOpenGL3Backend::EndFrame()`/`ReadPixels()`.
 *
 * That "immediately after" qualifier is load-bearing and is the deeper
 * discovery of this sub-phase: `RootBridge::BeginRootWindow()`'s
 * `ImGui::Begin()` call *queues* the root window's own background into
 * ImGui's draw list, but ImGui only *rasterizes* that queued draw list later,
 * in `EndFrame()`'s `ImGui::Render()` + `ImGui_ImplOpenGL3_RenderDrawData()`.
 * `NativeRendererGL3::Render()`, by contrast, draws **immediately** when
 * called. The result: calling `backend.EndFrame()` after `Show()` and reading
 * pixels *then* (as this test originally tried) shows the window's own
 * background painted over the `Box`, regardless of `Show()`/`Render()` having
 * run "first" in program order — deferred content always rasterizes after
 * immediate content within the same frame, inverting the intended z-order.
 * This is a real, deeper architectural gap than the clear-timing issue this
 * sub-phase fixed, and is **not** resolved here — see `.claude/DECISIONS.md`,
 * Phase 32.5, for the full analysis and what a real fix requires.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "GLFWOpenGL3Backend.hpp"
#include "NativeRendererGL3.hpp"
#include "Tree/Reconciler.hpp"

#include "ImFrame/Tree/Primitives/Box.hpp"

#include <glad/glad.h>

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <memory>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Internal;
using namespace ImFrame::Tree::Primitives;

namespace {

WindowConfig TestWindowConfig() {
    WindowConfig cfg{};
    cfg.Title  = "ReconcilerNativeRenderer_test";
    cfg.Width  = 128;
    cfg.Height = 128;
    return cfg;
}

struct Pixel { int r, g, b, a; };

/// Reads the currently-bound framebuffer directly — deliberately not `GLFWOpenGL3Backend::
/// ReadPixels()`, which only reflects state as of the *last* `EndFrame()` call. See this file's
/// header comment for why reading immediately, mid-frame, is the point of this test.
std::vector<unsigned char> ReadCurrentFramebuffer(int width, int height) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    // OpenGL's readback origin is bottom-left; flip rows so index 0 is top-left, matching every
    // other backend's ReadPixels() convention in this codebase.
    std::vector<unsigned char> flipped(pixels.size());
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    for (int y = 0; y < height; ++y) {
        std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(rowBytes * (height - 1 - y)), rowBytes,
                    flipped.begin() + static_cast<std::ptrdiff_t>(rowBytes * y));
    }
    return flipped;
}

Pixel Sample(const std::vector<unsigned char>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4;
    return {pixels[off + 0], pixels[off + 1], pixels[off + 2], pixels[off + 3]};
}

} // namespace

TEST_CASE("Reconciler + NativeRendererGL3: a Box's fill colour is drawn at the correct position "
          "immediately when Show() runs",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    // The real framebuffer size can differ from WindowConfig's requested size (this machine
    // enforces a wider minimum window width) — query it rather than assume.
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame(); // clears the framebuffer here (Phase 32.5) — before Show() draws into it

    Reconciler reconciler;
    reconciler.SetRenderer(std::make_unique<NativeRendererGL3>());

    const Tree::Widget root = Box().Width(64.0f).Height(64.0f).Background({0.0f, 1.0f, 0.0f, 1.0f});
    reconciler.Show(root);

    // Read immediately — before EndFrame()'s deferred ImGui rasterization repaints the root
    // window's own (queued-at-Begin, rasterized-at-EndFrame) background over what Show() just
    // drew. See this file's header comment.
    auto pixels = ReadCurrentFramebuffer(extent.Width, extent.Height);

    // The root window's content region starts a few pixels in from (0,0) (default ImGui window
    // padding) — (20, 20) is safely inside a 64x64 Box anchored at the content region's origin.
    const Pixel inside = Sample(pixels, 20, 20, extent.Width);
    REQUIRE(inside.g > 200);
    REQUIRE(inside.g > inside.r);
    REQUIRE(inside.a > 200);

    // Well outside the 64x64 Box, still inside the window — should be the clear colour, not green.
    const Pixel outside = Sample(pixels, extent.Width - 5, extent.Height - 5, extent.Width);
    const bool outsideLooksGreen = (outside.g > 200) && (outside.g > outside.r);
    REQUIRE_FALSE(outsideLooksGreen);

    backend.EndFrame(); // must still be called to keep ImGui's frame state balanced
    backend.Shutdown();
}
