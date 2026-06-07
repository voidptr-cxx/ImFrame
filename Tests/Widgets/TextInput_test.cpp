/**
 * @file     TextInput_test.cpp
 * @brief    Unit tests for Widgets::TextInput
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
#include "ImFrame/Widgets/TextInput.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::TextInput;

TEST_CASE("TextInput<string> does not crash and leaves value unchanged in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    std::string text = "hello";
    bool result = true;
    application.OnUi([&] {
        result = TextInput<std::string>("Name", text).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE_FALSE(result);
    REQUIRE(text == "hello");
}

TEST_CASE("TextInput<string> with hint does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    std::string text;
    application.OnUi([&] {
        TextInput<std::string>("Query", text).Hint("Search...").Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("TextInput<string> multiline does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    std::string text = "line1\nline2";
    application.OnUi([&] {
        TextInput<std::string>("Notes", text).Multiline().Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(text == "line1\nline2");
}

TEST_CASE("TextInput<string> password does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    std::string pw = "secret";
    application.OnUi([&] {
        TextInput<std::string>("Password", pw).Password().Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(pw == "secret");
}

TEST_CASE("TextInput<u8string> does not crash in headless frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    std::u8string text = u8"hello";
    application.OnUi([&] {
        TextInput<std::u8string>("U8Name", text).Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(text == u8"hello");
}

TEST_CASE("TextInput::OnChange not fired without interaction", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    std::string text = "unchanged";
    int changeCount = 0;
    application.OnUi([&] {
        TextInput<std::string>("Field", text)
            .OnChange([&](const std::string&) { ++changeCount; })
            .Show();
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(changeCount == 0);
}
