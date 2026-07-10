/**
 * @file     DockSpaceWidget_test.cpp
 * @brief    Behavioral tests for the Phase 30.3 `App::DockSpaceWidget` primitive
 *
 * Drives a real `HeadlessBackend` + `Application::SetRoot()` (the opt-in tree
 * path `DockSpaceWidget` is designed for — as opposed to `Application`'s
 * automatic `App::DockSpace`, exercised separately by `Tests/App/DockSpace_test.cpp`,
 * which this file does not touch).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-09
 * @version  2.6.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/App/DockSpaceWidget.hpp"

#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <imgui_internal.h>

using namespace ImFrame;
using namespace ImFrame::Tree;
using ImFrame::App::Application;
using ImFrame::App::DockSpaceWidget;

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
    return WindowConfig{.Title = "DockSpaceWidgetTest", .Width = 400, .Height = 300};
}

} // namespace

TEST_CASE("DockSpaceWidget: renders without error via SetRoot", "[tree][dockspace]") {
    struct Root {
        [[nodiscard]] Widget Build() const { return Widget(DockSpaceWidget()); }
    } root;

    int frame = 0;
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&frame](float) { ++frame; });

    const auto result = app.Run();

    REQUIRE(result.has_value());
    REQUIRE(frame == 3);
}

TEST_CASE("DockSpaceWidget: initialises the default layout on the first frame", "[tree][dockspace]") {
    struct Root {
        [[nodiscard]] Widget Build() const { return Widget(DockSpaceWidget()); }
    } root;

    // Application::RunOneFrame() runs OnUi() BEFORE the SetRoot() tree's
    // reconciler pass (the pass that actually mounts DockSpaceWidgetElement and
    // calls BeginDockSpaceWindow()) — so the window/node this test looks for
    // doesn't exist yet during frame 1's OnUi(). Use 2 frames and check on the
    // second, once frame 1's tree pass has run. Checking must happen from
    // inside OnUi() (a live ImGui frame) — ImGui::FindWindowByName() called
    // after Run() returns segfaults, since Run() tears down the ImGui context
    // before returning.
    bool nodeExists = false;
    int  frame      = 0;
    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    app.OnUi([&nodeExists, &frame] {
        ++frame;
        if (frame < 2) { return; }
        ImGuiWindow* win = ImGui::FindWindowByName("##ImFrameDockSpaceWidget");
        if (!win) { return; }
        // ImGui::GetID() is scoped to the current window's ID stack — reproduce
        // the hash DockSpaceWidgetElement::Paint() computed from inside its own
        // window (seed = that window's ID) rather than this callback's, which
        // runs inside Application's separate "##DockSpace" window.
        const ImGuiID dockId = ImHashStr("ImFrameDockSpaceWidget", 0, win->ID);
        nodeExists            = (ImGui::DockBuilderGetNode(dockId) != nullptr);
    });

    const auto result = app.Run();

    REQUIRE(result.has_value());
    REQUIRE(nodeExists);
}

TEST_CASE("DockSpaceWidget: MenuBar() round-trips through GetMenuBar()", "[tree][dockspace]") {
    DockSpaceWidget widget;
    REQUIRE_FALSE(widget.GetMenuBar());

    widget.MenuBar(true);
    REQUIRE(widget.GetMenuBar());

    widget.MenuBar(false);
    REQUIRE_FALSE(widget.GetMenuBar());
}
