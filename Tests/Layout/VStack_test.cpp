/**
 * @file     VStack_test.cpp
 * @brief    Unit tests for Layout::VStack
 *
 * @internal
 * Runs inside a headless ImGui context. Verifies that VStack::Render()
 * calls Show() on all widgets in order and does not crash.
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
#include "ImFrame/Layout/VStack.hpp"
#include "ImFrame/Widgets/Button.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Layout::VStack;
using ImFrame::Widgets::Button;

namespace {

struct CountWidget {
    int& count;
    explicit CountWidget(int& c) : count(c) {}
    bool Show() { ++count; return false; }
};

struct OrderWidget {
    std::vector<int>& order;
    int idx;
    OrderWidget(std::vector<int>& o, int i) : order(o), idx(i) {}
    bool Show() { order.push_back(idx); return false; }
};

} // namespace

TEST_CASE("VStack::Render calls Show on all widgets", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    int callCount = 0;
    app.OnUi([&] {
        CountWidget a{callCount}, b{callCount}, c{callCount};
        VStack().Render(a, b, c);
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(callCount == 3);
}

TEST_CASE("VStack::Render calls Show in top-to-bottom order", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    std::vector<int> order;
    app.OnUi([&] {
        OrderWidget a{order, 0}, b{order, 1}, c{order, 2};
        VStack().Render(a, b, c);
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(order == std::vector<int>{0, 1, 2});
}

TEST_CASE("VStack with custom spacing does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        VStack(12.0f).Render(
            Button("First"),
            Button("Second"),
            Button("Third")
        );
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("VStack with single widget does not insert Dummy", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    int callCount = 0;
    app.OnUi([&] {
        CountWidget a{callCount};
        VStack().Render(a);
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(callCount == 1);
}
