/**
 * @file     Separator_test.cpp
 * @brief    Unit tests for Widgets::Separator
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
#include "ImFrame/Widgets/Separator.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Separator;

TEST_CASE("Separator plain line does not crash and returns false", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool result = true;
    application.OnUi([&] {
        result = Separator().Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
}

TEST_CASE("Separator with label does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        Separator().Label("Settings Section").Show();
    });

    REQUIRE(application.Run().has_value());
}
