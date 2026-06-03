/**
 * @file     DockSpace_test.cpp
 * @brief    Unit tests for ImFrame::App::DockSpace
 *
 * @internal
 * Tests verify:
 * - A Begin() / End() pair inside a valid ImGui frame does not trigger ImGui
 *   assertions or leave the ImGui stack in an invalid state.
 * - The default layout is initialised on the first Begin() call: the DockBuilder
 *   node identified by "MainDockSpace" is non-null after Begin() returns.
 *
 * Tests are driven through Application::Run() using TestHeadlessBackend so that
 * a full ImGui frame context exists when DockSpace methods are called.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  0.8.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;

// ─── Begin/End pair ───────────────────────────────────────────────────────────

TEST_CASE("DockSpace Begin/End pair leaves ImGui stack in valid state", "[unit]") {
    // Application drives Begin()/End() automatically from RunOneFrame().
    // If the stack is corrupted ImGui's internal assertions fire and the test crashes.
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    bool uiReached = false;
    app.OnUi([&] {
        uiReached = true;
        // No additional work needed — DockSpace::Begin/End are called around this.
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

    app.OnUi([&] {
        // OnUi runs after DockSpace::Begin(), so the dockspace ID is resolvable
        // within the "##DockSpace" window context that Begin() opened.
        const ImGuiID id = ImGui::GetID("MainDockSpace");
        nodeExists = (ImGui::DockBuilderGetNode(id) != nullptr);
    });

    auto result = app.Run();

    REQUIRE(result.has_value());
    REQUIRE(nodeExists);
}
