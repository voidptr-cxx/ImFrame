/**
 * @file     Combo_test.cpp
 * @brief    Unit tests for Widgets::Combo
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
#include "ImFrame/Widgets/Combo.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Combo;

TEST_CASE("Combo<string> does not crash and returns false in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    static const std::string items[] = {"Alpha", "Beta", "Gamma"};
    std::string selected = "Alpha";
    bool result = true;

    application.OnUi([&] {
        result = Combo<std::string>("Mode", selected, items).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
    REQUIRE(selected == "Alpha");
}

TEST_CASE("Combo<string> with ItemLabel does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    static const std::string items[] = {"opt1", "opt2"};
    std::string selected = "opt1";

    application.OnUi([&] {
        Combo<std::string>("Pick", selected, items)
            .ItemLabel([](const std::string& s) { return s; })
            .Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("Combo<int> with format fallback does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    static const int items[] = {1, 2, 3, 4};
    int selected = 2;

    application.OnUi([&] {
        Combo<int>("Number", selected, items).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(selected == 2);
}
