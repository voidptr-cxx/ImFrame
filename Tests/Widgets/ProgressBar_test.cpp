/**
 * @file     ProgressBar_test.cpp
 * @brief    Unit tests for Widgets::ProgressBar
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
#include "ImFrame/Widgets/ProgressBar.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::ProgressBar;
using ImFrame::Widgets::Vec2;

TEST_CASE("ProgressBar::Show always returns false", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool result = true;
    application.OnUi([&] {
        result = ProgressBar(0.6f).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
}

TEST_CASE("ProgressBar with overlay does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        ProgressBar(0.75f).Overlay("Loading...").Size(Vec2{-1.0f, 0.0f}).Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("ProgressBar edge fractions (0 and 1) do not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        ProgressBar(0.0f).Show();
        ProgressBar(1.0f).Show();
    });

    REQUIRE(application.Run().has_value());
}
