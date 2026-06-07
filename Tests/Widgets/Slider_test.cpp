/**
 * @file     Slider_test.cpp
 * @brief    Unit tests for Widgets::Slider
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
#include "ImFrame/Widgets/Slider.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Slider;

TEST_CASE("Slider<int> does not crash and returns false in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    int val = 5;
    bool result = true;
    application.OnUi([&] {
        result = Slider<int>("Count", val, 0, 10).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
    REQUIRE(val == 5);
}

TEST_CASE("Slider<float> does not crash and returns false in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    float val = 0.5f;
    bool result = true;
    application.OnUi([&] {
        result = Slider<float>("Volume", val, 0.0f, 1.0f).Format("%.2f").Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
}

TEST_CASE("Slider<double> does not crash in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    double val = 0.5;
    application.OnUi([&] {
        Slider<double>("Precision", val, 0.0, 1.0).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(val == 0.5);
}

TEST_CASE("Slider<unsigned char> (narrow int) does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    unsigned char val = 128;
    application.OnUi([&] {
        Slider<unsigned char>("Byte", val, 0, 255).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(val == 128);
}

TEST_CASE("Slider::Disabled does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    int val = 3;
    application.OnUi([&] {
        Slider<int>("Locked", val, 0, 10).Disabled().Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(val == 3);
}
