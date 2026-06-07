/**
 * @file     Radio_test.cpp
 * @brief    Unit tests for Widgets::Radio
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
#include "ImFrame/Widgets/Radio.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Radio;

TEST_CASE("Radio::Show does not crash in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    int mode = 0;
    application.OnUi([&] {
        Radio("Option A", mode, 0).Show();
        Radio("Option B", mode, 1).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(mode == 0); // no interaction — value unchanged
}

TEST_CASE("Radio group shares binding without crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    int selection = 1;
    application.OnUi([&] {
        Radio("X", selection, 0).Show();
        Radio("Y", selection, 1).Show();
        Radio("Z", selection, 2).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(selection == 1);
}
