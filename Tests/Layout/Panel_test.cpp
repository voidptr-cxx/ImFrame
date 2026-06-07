/**
 * @file     Panel_test.cpp
 * @brief    Unit tests for Layout::Panel
 *
 * @internal
 * Runs inside a headless ImGui context driven by TestHeadlessBackend.
 * Panel::Begin() is called each frame; the ChildScope RAII destructor
 * exercises the EndChild() path on every test.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "../App/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Layout/Panel.hpp"
#include "ImFrame/Widgets/Text.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Layout::Panel;
using ImFrame::Widgets::Vec2;
using ImFrame::Widgets::Vec4;

TEST_CASE("Panel::Begin returns a scope that does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto scope = Panel("test_panel").Begin();
        (void)scope;
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("Panel::Begin with inner widgets does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        if (auto scope = Panel("content").Size(Vec2{200.0f, 100.0f}).Begin()) {
            ImFrame::Widgets::Text("Hello from panel").Show();
        }
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("Panel::Border false does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto scope = Panel("borderless").Border(false).Begin();
        (void)scope;
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("Panel::Padding does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto scope = Panel("padded").Padding(Vec2{8.0f, 8.0f}).Begin();
        (void)scope;
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("Panel::Background does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto scope = Panel("colored").Background(Vec4{0.1f, 0.1f, 0.1f, 1.0f}).Begin();
        (void)scope;
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("Nested Panels do not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        if (auto outer = Panel("outer").Size(Vec2{300.0f, 200.0f}).Begin()) {
            if (auto inner = Panel("inner").Size(Vec2{150.0f, 80.0f}).Begin()) {
                ImFrame::Widgets::Text("Nested").Show();
            }
        }
    });

    REQUIRE(app.Run().has_value());
}
