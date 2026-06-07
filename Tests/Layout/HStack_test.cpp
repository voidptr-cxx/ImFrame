/**
 * @file     HStack_test.cpp
 * @brief    Unit tests for Layout::HStack
 *
 * @internal
 * Runs inside a headless ImGui context. Verifies that HStack::Render()
 * calls Show() on all widgets and does not crash. Position deltas cannot be
 * verified in a headless context (no renderer), so we test via call count.
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
#include "ImFrame/Layout/HStack.hpp"
#include "ImFrame/Widgets/Button.hpp"
#include "ImFrame/Widgets/Text.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Layout::HStack;
using ImFrame::Widgets::Button;

namespace {

/// Minimal Renderable stub that counts Show() calls.
struct CountWidget {
    int& count;
    explicit CountWidget(int& c) : count(c) {}
    bool Show() { ++count; return false; }
};

} // namespace

TEST_CASE("HStack::Render calls Show on all widgets", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    int callCount = 0;
    app.OnUi([&] {
        CountWidget a{callCount}, b{callCount}, c{callCount};
        HStack().Render(a, b, c);
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(callCount == 3);
}

TEST_CASE("HStack::Render with one widget does not insert SameLine", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    int callCount = 0;
    app.OnUi([&] {
        CountWidget a{callCount};
        HStack().Render(a);
    });

    REQUIRE(app.Run().has_value());
    REQUIRE(callCount == 1);
}

TEST_CASE("HStack with explicit spacing does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        HStack(16.0f).Render(
            Button("Cancel"),
            Button("OK")
        );
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("HStack with default spacing does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    app.OnUi([&] {
        HStack().Render(
            Button("A"),
            Button("B"),
            Button("C")
        );
    });

    REQUIRE(app.Run().has_value());
}
