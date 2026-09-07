/**
 * @file     Primitives_test.cpp
 * @brief    Behavioral tests for the eight Tree::Primitives via HeadlessBackend
 *
 * Uses `Application::SetRoot()` to drive a real `Reconciler::Show()` pass
 * (Mount/Layout/Paint) each frame against the `HeadlessBackend`'s ImGui
 * context. `GestureRegion`'s hover/click tests drive ImGui's input queue
 * directly (`ImGuiIO::AddMousePosEvent`/`AddMouseButtonEvent`) since
 * `Application::InjectInputEvent` only round-trips the backend's own event
 * queue and does not feed ImGui's internal item hover/click state.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Tree/Primitives/Box.hpp"
#include "ImFrame/Tree/Primitives/Expanded.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/GestureRegion.hpp"
#include "ImFrame/Tree/Primitives/SizedBox.hpp"
#include "ImFrame/Tree/Primitives/Spacer.hpp"
#include "ImFrame/Tree/Primitives/Stack.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"

#include <catch2/catch_test_macros.hpp>
#include <imgui.h>

using namespace ImFrame;
using namespace ImFrame::Tree;
using namespace ImFrame::Tree::Primitives;
using ImFrame::App::Application;

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
    return WindowConfig{.Title = "PrimitivesTest", .Width = 400, .Height = 300};
}

} // namespace

// ─── Each primitive renders without error ─────────────────────────────────────

TEST_CASE("Box renders without error", "[unit]") {
    struct Root {
        [[nodiscard]] Widget Build() const {
            return Box().Padding(Widgets::EdgeInsets::All(4.0f))
                        .Background({0.2f, 0.2f, 0.2f, 1.0f})
                        .Child(Text("hi"));
        }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

TEST_CASE("Flex with Stack and SizedBox children renders without error", "[unit]") {
    struct Root {
        [[nodiscard]] Widget Build() const {
            return Flex(Flex::Axis::Horizontal)
                .Gap(4.0f)
                .Children({
                    SizedBox().Width(10.0f).Height(10.0f),
                    Stack().Children({Text("a"), Text("b")}),
                    Spacer(),
                    Expanded(Text("c")).Factor(1),
                });
        }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

TEST_CASE("GestureRegion without a child renders without error", "[unit]") {
    struct Root {
        [[nodiscard]] Widget Build() const { return GestureRegion(); }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

// ─── GestureRegion input ───────────────────────────────────────────────────────

namespace {

struct ClickTracker {
    bool clicked         = false;
    int  hoverEnterCount = 0;
    int  hoverExitCount  = 0;
};

} // namespace

// `Application::Run()` calls `IBackend::Init()` (which creates the ImGui context)
// before its internal `RunOneFrame()` loop — calling `RunOneFrame()` directly
// without going through `Run()` first dereferences a null ImGui context. To
// inject ImGui IO events at specific frames while still going through `Run()`,
// these tests drive a frame counter from `OnUpdate()`, which fires once per
// frame *before* `BeginFrame()` — exactly when a queued `AddMousePosEvent`/
// `AddMouseButtonEvent` needs to be queued for that frame's `NewFrame()` to pick up.

TEST_CASE("GestureRegion::OnHover fires true on enter and false on exit", "[unit]") {
    ClickTracker tracker;
    int          frame = 0;

    struct Root {
        ClickTracker* tracker;
        [[nodiscard]] Widget Build() const {
            return GestureRegion()
                .OnHover([t = tracker](bool hovered) {
                    if (hovered) { t->hoverEnterCount++; } else { t->hoverExitCount++; }
                })
                .Child(SizedBox().Width(100.0f).Height(100.0f));
        }
    } root{&tracker};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(5), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) {
        ++frame;
        if (frame == 2) { ImGui::GetIO().AddMousePosEvent(10.0f, 10.0f); }      // inside the region
        if (frame == 4) { ImGui::GetIO().AddMousePosEvent(-100.0f, -100.0f); } // outside the region
    });
    REQUIRE(app.Run().has_value());
    REQUIRE(tracker.hoverEnterCount == 1);
    REQUIRE(tracker.hoverExitCount == 1);
}

TEST_CASE("GestureRegion::OnClick fires on a press-then-release over the region", "[unit]") {
    ClickTracker tracker;
    int          frame = 0;

    struct Root {
        ClickTracker* tracker;
        [[nodiscard]] Widget Build() const {
            return GestureRegion()
                .OnClick([t = tracker]() { t->clicked = true; })
                .Child(SizedBox().Width(100.0f).Height(100.0f));
        }
    } root{&tracker};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(6), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) {
        ++frame;
        if (frame == 2) { ImGui::GetIO().AddMousePosEvent(10.0f, 10.0f); }
        if (frame == 3) { ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true); }
        if (frame == 4) { ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false); }
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(tracker.clicked);
}
