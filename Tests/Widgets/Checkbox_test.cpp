/**
 * @file     Checkbox_test.cpp
 * @brief    Unit tests for Widgets::Checkbox
 *
 * @internal
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
#include "ImFrame/Widgets/Checkbox.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Checkbox;

TEST_CASE("Checkbox::Show does not crash in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool value = false;
    bool result = true;
    application.OnUi([&] {
        result = Checkbox("Enable", value).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
    REQUIRE_FALSE(value); // unchanged — no interaction
}

TEST_CASE("Checkbox::OnChange is not fired without interaction", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool value = false;
    int  changeCount = 0;
    application.OnUi([&] {
        Checkbox("Toggle", value)
            .OnChange([&](bool) { ++changeCount; })
            .Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(changeCount == 0);
}

TEST_CASE("Checkbox::Disabled does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool value = true;
    application.OnUi([&] {
        Checkbox("Locked", value).Disabled().Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(value); // unchanged
}
