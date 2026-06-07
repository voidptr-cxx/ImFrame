/**
 * @file     ColorEdit_test.cpp
 * @brief    Unit tests for Widgets::ColorEdit
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
#include "ImFrame/Widgets/ColorEdit.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::ColorEdit;
using ImFrame::Widgets::Vec4;

TEST_CASE("ColorEdit::Show does not crash in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    Vec4 color{1.0f, 0.5f, 0.0f, 1.0f};
    bool result = true;

    application.OnUi([&] {
        result = ColorEdit("Tint", color).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
    REQUIRE(color.x == 1.0f); // unchanged
}

TEST_CASE("ColorEdit::Alpha does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    Vec4 color{0.2f, 0.4f, 0.8f, 0.5f};
    application.OnUi([&] {
        ColorEdit("Background", color).Alpha(true).Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("ColorEdit::OnChange not fired without interaction", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    Vec4 color{};
    int changeCount = 0;
    application.OnUi([&] {
        ColorEdit("Hue", color)
            .OnChange([&](Vec4) { ++changeCount; })
            .Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(changeCount == 0);
}
