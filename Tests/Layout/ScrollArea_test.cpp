/**
 * @file     ScrollArea_test.cpp
 * @brief    Unit tests for Layout::ScrollArea
 *
 * @internal
 * Runs inside a headless ImGui context. Scroll position queries return
 * zero in a headless frame (no content has been laid out), so tests verify
 * that the API can be called without crashing and that the RAII scope
 * correctly calls EndChild() on destruction.
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
#include "ImFrame/Layout/ScrollArea.hpp"
#include "ImFrame/Widgets/Text.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Layout::ScrollArea;
using ImFrame::Widgets::Vec2;

TEST_CASE("ScrollArea::Begin returns a scope that does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto scope = ScrollArea("log").Begin();
        (void)scope;
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("ScrollArea with inner content does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        if (auto scope = ScrollArea("content").Size(Vec2{300.0f, 200.0f}).Begin()) {
            ImFrame::Widgets::Text("Line 1").Show();
            ImFrame::Widgets::Text("Line 2").Show();
        }
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("ScrollArea::HorizontalBar does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto scope = ScrollArea("hscroll").HorizontalBar(true).Begin();
        (void)scope;
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("ScrollArea::VerticalBar false does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto scope = ScrollArea("novscroll").VerticalBar(false).Begin();
        (void)scope;
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("ScrollArea::ScrollToBottom does not crash inside scope", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto area = ScrollArea("auto_scroll").Size(Vec2{200.0f, 100.0f});
        if (auto scope = area.Begin()) {
            ImFrame::Widgets::Text("Content").Show();
            area.ScrollToBottom();
        }
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("ScrollArea::ScrollPosition returns zero in headless frame", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    Vec2 pos{1.0f, 1.0f};
    app.OnUi([&] {
        auto area = ScrollArea("pos_check");
        if (auto scope = area.Begin()) {
            pos = area.ScrollPosition();
        }
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(pos.x == 0.0f);
    REQUIRE(pos.y == 0.0f);
}

TEST_CASE("ScrollArea::SetScrollPosition does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        auto area = ScrollArea("programmatic");
        if (auto scope = area.Begin()) {
            area.SetScrollPosition(Vec2{0.0f, 50.0f});
        }
    });

    REQUIRE(app.Run().has_value());
}
