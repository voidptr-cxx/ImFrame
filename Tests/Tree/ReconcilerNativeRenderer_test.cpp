/**
 * @file     ReconcilerNativeRenderer_test.cpp
 * @brief    Proves NativeRendererGL3::RenderMode::DeferredReplay fixes the Phase 32.5 compositing bug
 *
 * @internal
 * Drives `Internal::Reconciler::Show()` — the actual production call site, via
 * `RootBridge::BeginRootWindow()`, exactly as `Application::Run()`'s frame
 * tick does — with `Internal::NativeRendererGL3` swapped in via
 * `SetRenderer()`, through a real `GLFWOpenGL3Backend`, running the *complete*
 * `BeginFrame() -> Show() -> EndFrame()` frame lifecycle (unlike this file's
 * Phase 32.5 version, which deliberately read pixels mid-frame to sidestep
 * the bug it was documenting rather than fixing).
 *
 * Phase 32.5 discovered: `RootBridge::BeginRootWindow()`'s `ImGui::Begin()`
 * only *queues* the root window's own background into ImGui's draw list —
 * ImGui does not *rasterize* that queued content until `EndFrame()`'s
 * `ImGui::Render()` + `ImGui_ImplOpenGL3_RenderDrawData()`. A
 * `RenderMode::Immediate` draw (issued synchronously, mid-frame, inside
 * `Show()`) always rasterizes *before* that deferred background, so the
 * background silently paints over it by the time `EndFrame()` finishes —
 * regardless of `Show()` having run "first" in program order. The first
 * `TEST_CASE` below reproduces exactly that: read pixels after the full
 * `EndFrame()`, and the `Immediate`-mode Box is gone.
 *
 * `RenderMode::DeferredReplay` (Phase 32.6) fixes this by queuing an
 * `ImDrawList::AddCallback()` on the root window's own draw list instead of
 * drawing synchronously — `ImGui_ImplOpenGL3_RenderDrawData()` then invokes
 * that callback at the exact point in the draw list where it was recorded,
 * i.e. *after* the window's own (also-deferred) background, giving the
 * correct z-order automatically. The second `TEST_CASE` below proves this:
 * the same Box, same full frame lifecycle, same post-`EndFrame()` pixel read
 * — but this time it survives.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-02
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

#include <catch2/catch_test_macros.hpp>
#include <memory>

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

Pixel Sample(const std::vector<std::byte>& pixels, int x, int y, int width) {
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4;
    return {
        static_cast<int>(pixels[off + 0]),
        static_cast<int>(pixels[off + 1]),
        static_cast<int>(pixels[off + 2]),
        static_cast<int>(pixels[off + 3]),
    };
}

bool LooksGreen(const Pixel& p) { return p.g > 200 && p.g > p.r && p.a > 200; }

} // namespace

TEST_CASE("Reconciler + NativeRendererGL3::RenderMode::Immediate: the root window's deferred "
          "background still erases the Box by the time EndFrame() finishes (Phase 32.5's bug, "
          "reproduced end-to-end rather than sidestepped)",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame();

    Reconciler reconciler;
    reconciler.SetRenderer(std::make_unique<NativeRendererGL3>(NativeRendererGL3::RenderMode::Immediate));

    const Tree::Widget root = Box().Width(64.0f).Height(64.0f).Background({0.0f, 1.0f, 0.0f, 1.0f});
    reconciler.Show(root);

    backend.EndFrame();
    auto pixels = backend.ReadPixels();

    // The root window's content region starts a few pixels in from (0,0) (default ImGui window
    // padding) — (20, 20) is safely inside a 64x64 Box anchored at the content region's origin.
    const Pixel inside = Sample(pixels, 20, 20, extent.Width);
    REQUIRE_FALSE(LooksGreen(inside)); // the documented bug: background painted over it

    backend.Shutdown();
}

TEST_CASE("Reconciler + NativeRendererGL3::RenderMode::DeferredReplay: the Box survives the root "
          "window's deferred rasterization and is visible after a full EndFrame()",
          "[unit]") {
    GLFWOpenGL3Backend backend;
    REQUIRE(backend.Init(TestWindowConfig()).has_value());
    const WindowExtent extent = backend.WindowSize();

    backend.Poll();
    backend.BeginFrame();

    Reconciler reconciler;
    reconciler.SetRenderer(std::make_unique<NativeRendererGL3>(NativeRendererGL3::RenderMode::DeferredReplay));

    const Tree::Widget root = Box().Width(64.0f).Height(64.0f).Background({0.0f, 1.0f, 0.0f, 1.0f});
    reconciler.Show(root);

    backend.EndFrame();
    auto pixels = backend.ReadPixels();

    const Pixel inside = Sample(pixels, 20, 20, extent.Width);
    REQUIRE(LooksGreen(inside)); // fixed: correctly composited on top of the deferred background

    // Well outside the 64x64 Box, still inside the window — should be the window background, not green.
    const Pixel outside = Sample(pixels, extent.Width - 5, extent.Height - 5, extent.Width);
    REQUIRE_FALSE(LooksGreen(outside));

    backend.Shutdown();
}
