/**
 * @file     Viewport_test.cpp
 * @brief    Unit tests for Viewport registration, dispatch timing, resize, and input hooks
 *
 * @internal
 * Tests verify:
 * - OnRender fires starting from the second frame (frame 0 registers the viewport;
 *   DispatchRenders fires from frame 1 onward when wasShown becomes true).
 * - OnRender is skipped entirely when Show() is never called.
 * - OnResize fires synchronously on first Show (initial dimensions) and again
 *   when the requested size changes between frames.
 * - HasInputCallback() reflects whether an OnInput callback was registered.
 * - Viewport destructor does not crash after Application::Run() returns.
 * - Two Viewports with distinct IDs receive independent render callbacks.
 *
 * Uses FrameLimitedHeadlessBackend — a HeadlessBackend subclass with a configurable
 * frame limit — so tests are finite and CreateViewportFramebuffer() is available.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Backends/InputEvent.hpp"
#include "ImFrame/Rendering/Viewport.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Rendering;
using ImFrame::App::Application;

// ─── Test helper ─────────────────────────────────────────────────────────────

namespace {

/// HeadlessBackend subclass that stops the render loop after N full frames.
class FrameLimitedHeadlessBackend final : public Internal::HeadlessBackend {
public:
    explicit FrameLimitedHeadlessBackend(int maxFrames) : _maxFrames(maxFrames) {}

    FrameInfo Poll() override {
        FrameInfo info   = Internal::HeadlessBackend::Poll();
        info.ShouldClose = (_frameCount >= _maxFrames);
        return info;
    }

    void BeginFrame(WindowHandle h = PrimaryWindow) override {
        Internal::HeadlessBackend::BeginFrame(h);
        ++_frameCount;
    }

private:
    int _maxFrames;
    int _frameCount = 0;
};

WindowConfig TestConfig() {
    return WindowConfig{ .Title = "ViewportTest", .Width = 320, .Height = 240 };
}

} // anonymous namespace

// ─── OnRender dispatch timing ─────────────────────────────────────────────────

TEST_CASE("Viewport OnRender fires starting from the second frame", "[unit]")
{
    // Frame 0: Show() registers the viewport; FlipShownFlags promotes wasShown=true.
    // Frames 1–2: DispatchRenders sees wasShown=true and fires OnRender. Total = 2.
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());

    HeadlessViewport vp("timing_test");
    int renderCount = 0;
    vp.OnRender([&](const RenderContext&) { ++renderCount; });

    app.OnUi([&] { vp.Show(); });

    REQUIRE(app.Run().has_value());
    REQUIRE(renderCount == 2);
}

TEST_CASE("Viewport OnRender is not called when Show is never called", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());

    HeadlessViewport vp("never_shown");
    int renderCount = 0;
    vp.OnRender([&](const RenderContext&) { ++renderCount; });

    app.OnUi([&] { /* Show() intentionally omitted */ });

    REQUIRE(app.Run().has_value());
    REQUIRE(renderCount == 0);
}

TEST_CASE("Viewport render count is N-1 for N frames", "[unit]")
{
    // Frame 0 registers; frames 1-4 render.
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(5), TestConfig());

    HeadlessViewport vp("scale_test");
    int renderCount = 0;
    vp.OnRender([&](const RenderContext&) { ++renderCount; });

    app.OnUi([&] { vp.Show(); });

    REQUIRE(app.Run().has_value());
    REQUIRE(renderCount == 4);
}

// ─── OnResize ─────────────────────────────────────────────────────────────────

TEST_CASE("Viewport OnResize fires on first Show with the initial dimensions", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());

    HeadlessViewport vp("resize_init");
    vp.Size({64.0f, 64.0f});
    Widgets::Vec2 lastSize{0.0f, 0.0f};
    vp.OnResize([&](Widgets::Vec2 s) { lastSize = s; });

    app.OnUi([&] { vp.Show(); });

    REQUIRE(app.Run().has_value());
    REQUIRE(lastSize.X == 64.0f);
    REQUIRE(lastSize.Y == 64.0f);
}

TEST_CASE("Viewport OnResize fires when explicit size changes between Shows", "[unit]")
{
    // Frame 0: GetOrCreate fires OnResize({64,64}).
    // Frame 2: GetOrCreate detects dimension change, fires OnResize({128,128}).
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(4), TestConfig());

    HeadlessViewport vp("resize_change");
    int resizeCount = 0;
    Widgets::Vec2 lastSize{0.0f, 0.0f};
    vp.OnResize([&](Widgets::Vec2 s) { lastSize = s; ++resizeCount; });

    int frame = 0;
    app.OnUi([&] {
        if (frame < 2)
            vp.Size({64.0f, 64.0f});
        else
            vp.Size({128.0f, 128.0f});
        vp.Show();
        ++frame;
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(resizeCount == 2);
    REQUIRE(lastSize.X == 128.0f);
    REQUIRE(lastSize.Y == 128.0f);
}

// ─── Input callback registration ─────────────────────────────────────────────

TEST_CASE("Viewport HasInputCallback is false before OnInput is registered", "[unit]")
{
    Viewport vp("input_pre");
    REQUIRE(!vp.HasInputCallback());
}

TEST_CASE("Viewport HasInputCallback is true after OnInput is registered", "[unit]")
{
    Viewport vp("input_post");
    vp.OnInput([](const Backends::InputEvent&) {});
    REQUIRE(vp.HasInputCallback());
}

// ─── Destructor safety ────────────────────────────────────────────────────────

TEST_CASE("Viewport destructor after Run does not crash", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    {
        HeadlessViewport vp("dtor_test");
        app.OnUi([&] { vp.Show(); });
        REQUIRE(app.Run().has_value());
        // vp destroyed here — ~Viewport() must not crash.
    }
    // app destroyed here — must not crash after registry was already cleaned up.
}

// ─── Multiple independent viewports ──────────────────────────────────────────

TEST_CASE("Two Viewports with different IDs receive independent render callbacks", "[unit]")
{
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());

    HeadlessViewport vpA("vp_a");
    HeadlessViewport vpB("vp_b");
    int countA = 0;
    int countB = 0;
    vpA.OnRender([&](const RenderContext&) { ++countA; });
    vpB.OnRender([&](const RenderContext&) { ++countB; });

    app.OnUi([&] { vpA.Show(); vpB.Show(); });

    REQUIRE(app.Run().has_value());
    REQUIRE(countA == 2);
    REQUIRE(countB == 2);
}
