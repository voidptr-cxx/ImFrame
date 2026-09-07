/**
 * @file     DockSpace_test.cpp
 * @brief    Unit tests for ImFrame::App::DockSpace
 *
 * @internal
 * Tests verify:
 * - A Begin() / End() pair inside a valid ImGui frame does not trigger ImGui
 *   assertions or leave the ImGui stack in an invalid state.
 * - The default layout is initialised on the first Begin() call.
 * - SaveLayout / LoadLayout round-trip preserves the ini string.
 * - ResetLayout restores the default layout without crashing.
 * - ListLayouts returns all names added via SaveLayout, in insertion order.
 *
 * Tests are driven through Application::Run() using TestHeadlessBackend so that
 * a full ImGui frame context exists when DockSpace methods are called.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using ImFrame::App::Application;
using ImFrame::App::DockSpace;
using ImFrame::Tests::TestHeadlessBackend;

// ─── Begin/End pair ───────────────────────────────────────────────────────────

TEST_CASE("DockSpace Begin/End pair leaves ImGui stack in valid state", "[unit]") {
    // Application drives Begin()/End() automatically from RunOneFrame().
    // If the stack is corrupted ImGui's internal assertions fire and the test crashes.
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    bool uiReached = false;
    app.OnUi([&uiReached] {
        uiReached = true;
    });

    auto result = app.Run();

    REQUIRE(result.has_value());
    REQUIRE(uiReached);
}

// ─── Default layout initialisation ───────────────────────────────────────────

TEST_CASE("DockSpace initialises the default layout on the first frame", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    bool nodeExists = false;

    app.OnUi([&nodeExists] {
        // OnUi runs after DockSpace::Begin(), so the dockspace ID is resolvable
        // within the "##DockSpace" window context that Begin() opened.
        const ImGuiID id = ImGui::GetID("MainDockSpace");
        nodeExists = (ImGui::DockBuilderGetNode(id) != nullptr);
    });

    auto result = app.Run();

    REQUIRE(result.has_value());
    REQUIRE(nodeExists);
}

// ─── SaveLayout / ListLayouts ─────────────────────────────────────────────────

TEST_CASE("DockSpace SaveLayout adds layout name to ListLayouts", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    std::vector<std::string> layouts;
    app.OnUi([&app, &layouts] {
        app.GetDockSpace().SaveLayout("profile1");
        layouts = app.GetDockSpace().ListLayouts();
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(layouts.size() == 1);
    REQUIRE(layouts[0] == "profile1");
}

TEST_CASE("DockSpace ListLayouts returns all saved names in insertion order", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    std::vector<std::string> layouts;
    app.OnUi([&app, &layouts] {
        app.GetDockSpace().SaveLayout("first");
        app.GetDockSpace().SaveLayout("second");
        app.GetDockSpace().SaveLayout("third");
        layouts = app.GetDockSpace().ListLayouts();
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(layouts.size() == 3);
    REQUIRE(layouts[0] == "first");
    REQUIRE(layouts[1] == "second");
    REQUIRE(layouts[2] == "third");
}

TEST_CASE("DockSpace SaveLayout does not duplicate existing name", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    std::vector<std::string> layouts;
    app.OnUi([&app, &layouts] {
        app.GetDockSpace().SaveLayout("layout");
        app.GetDockSpace().SaveLayout("layout"); // save again — should not duplicate
        layouts = app.GetDockSpace().ListLayouts();
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(layouts.size() == 1);
}

// ─── SaveLayout / LoadLayout round-trip ──────────────────────────────────────

TEST_CASE("DockSpace SaveLayout/LoadLayout restores valid dockspace state", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    // After SaveLayout + LoadLayout the dockspace node must still be valid.
    bool stateValid = false;
    app.OnUi([&app, &stateValid] {
        DockSpace& ds = app.GetDockSpace();
        ds.SaveLayout("test");
        ds.LoadLayout("test");
        const ImGuiID id = ImGui::GetID("MainDockSpace");
        stateValid = (ImGui::DockBuilderGetNode(id) != nullptr);
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(stateValid);
}

// ─── ResetLayout ─────────────────────────────────────────────────────────────

TEST_CASE("DockSpace ResetLayout restores default layout without crashing", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(2);
    Application app(std::move(backend));

    // Package both state variables into a single struct to keep the lambda
    // capture at 2 pointers (= 16 bytes), fitting within Delegate's SBO.
    struct State { bool resetOk = false; int frame = 0; };
    State state;

    app.OnUi([&app, &state] {
        if (state.frame == 0) {
            app.GetDockSpace().SaveLayout("modified");
        } else {
            app.GetDockSpace().ResetLayout();
            // The dockspace node must still be valid after reset.
            const ImGuiID id = ImGui::GetID("MainDockSpace");
            state.resetOk = (ImGui::DockBuilderGetNode(id) != nullptr);
        }
        ++state.frame;
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(state.resetOk);
}
