/**
 * @file     Button_test.cpp
 * @brief    Unit tests for Widgets::Button
 *
 * @internal
 * Runs inside a headless ImGui context driven by TestHeadlessBackend.
 * In a no-interaction frame the button is never clicked, so Show() returns
 * false and OnClick is never fired.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "../App/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Widgets/Button.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Button;
using ImFrame::Widgets::Vec2;

TEST_CASE("Button::Show does not crash and returns false in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool showResult = true; // start true to confirm it is set false
    application.OnUi([&] {
        showResult = Button("Test Button").Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(showResult);
}

TEST_CASE("Button::OnClick is not fired without user interaction", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    int clickCount = 0;
    application.OnUi([&] {
        Button("Click Me").OnClick([&] { ++clickCount; }).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(clickCount == 0);
}

TEST_CASE("Button::Disabled does not crash and returns false", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool result = true;
    application.OnUi([&] {
        result = Button("Disabled").Disabled().Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
}

TEST_CASE("Button::Icon does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        Button("Save").Icon("\xef\x80\x95").Size(Vec2{120.0f, 0.0f}).Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("Button fluent builder returns same object", "[unit]") {
    // Verifies the builder chain compiles and the returned reference is *this.
    bool called = false;
    Button btn("OK");
    Button& ref1 = btn.OnClick([&] { called = true; });
    Button& ref2 = ref1.Disabled(false);
    Button& ref3 = ref2.Tooltip("Press to confirm");
    Button& ref4 = ref3.Width(100.0f);
    Button& ref5 = ref4.Id("ok_btn");
    REQUIRE(&btn == &ref5);
}
