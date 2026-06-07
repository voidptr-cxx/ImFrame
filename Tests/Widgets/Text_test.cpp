/**
 * @file     Text_test.cpp
 * @brief    Unit tests for Widgets::Text
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
#include "ImFrame/Widgets/Text.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Text;
using ImFrame::Widgets::Vec4;

TEST_CASE("Text plain always returns false and does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool result = true;
    application.OnUi([&] {
        result = Text("Hello, ImFrame!").Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
}

TEST_CASE("Text::Colored does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        Text("Warning").Colored(Vec4{1.0f, 0.5f, 0.0f, 1.0f}).Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("Text::Wrapped does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        Text("This is a long paragraph that wraps to the next line.").Wrapped().Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("Text::Disabled does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        Text("Greyed out text").Disabled().Show();
    });

    REQUIRE(application.Run().has_value());
}
