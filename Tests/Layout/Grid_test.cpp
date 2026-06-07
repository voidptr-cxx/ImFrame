/**
 * @file     Grid_test.cpp
 * @brief    Unit tests for Layout::Grid
 *
 * @internal
 * Runs inside a headless ImGui context. Verifies that Grid::Render()
 * calls Show() on all widgets and does not crash. Row-wrap behaviour and
 * column positions cannot be asserted without a renderer, so tests focus on
 * call count and no-crash guarantees.
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
#include "ImFrame/Layout/Grid.hpp"
#include "ImFrame/Widgets/Button.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Layout::Grid;
using ImFrame::Widgets::Button;

namespace {

struct CountWidget {
    int& count;
    explicit CountWidget(int& c) : count(c) {}
    bool Show() { ++count; return false; }
};

} // namespace

TEST_CASE("Grid::Render calls Show on all widgets", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    int callCount = 0;
    app.OnUi([&] {
        CountWidget a{callCount}, b{callCount}, c{callCount}, d{callCount};
        Grid(2).Render(a, b, c, d);
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(callCount == 4);
}

TEST_CASE("Grid fills columns left-to-right without crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        Grid(3).Render(
            Button("A"), Button("B"), Button("C"),
            Button("D"), Button("E"), Button("F")
        );
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("Grid with single column does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        Grid(1).Render(Button("Only"), Button("Two"), Button("Three"));
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("Grid with custom spacing does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        Grid(2).Spacing(4.0f).Id("spaced_grid").Render(
            Button("X"), Button("Y")
        );
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("Grid with custom Id does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        Grid(2).Id("grid_a").Render(Button("1"), Button("2"));
        Grid(2).Id("grid_b").Render(Button("3"), Button("4"));
    });

    REQUIRE(app.Run().has_value());
}
