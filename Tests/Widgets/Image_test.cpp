/**
 * @file     Image_test.cpp
 * @brief    Unit tests for Widgets::Image
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
#include "ImFrame/Widgets/Image.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Image;
using ImFrame::Widgets::Vec2;
using ImFrame::Widgets::Vec4;

TEST_CASE("Image::Show always returns false", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool result = true;
    application.OnUi([&] {
        // Use texture ID 1 — the font atlas is set to ID 1 in the headless backend.
        result = Image(reinterpret_cast<void*>(1), Vec2{64.0f, 64.0f}).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
}

TEST_CASE("Image::ShowButton does not crash in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool clicked = true;
    application.OnUi([&] {
        clicked = Image(reinterpret_cast<void*>(1), Vec2{32.0f, 32.0f})
                      .Id("test_btn")
                      .ShowButton();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(clicked);
}

TEST_CASE("Image::Tint and UV settings do not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        Image(reinterpret_cast<void*>(1), Vec2{128.0f, 128.0f})
            .UV0({0.0f, 0.0f})
            .UV1({0.5f, 0.5f})
            .Tint({1.0f, 1.0f, 1.0f, 0.8f})
            .BorderColor({0.0f, 0.0f, 0.0f, 1.0f})
            .Show();
    });

    REQUIRE(application.Run().has_value());
}
