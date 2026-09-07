/**
 * @file     Theme_test.cpp
 * @brief    Unit tests for Theme::Apply() and ThemeBuilder
 *
 * @internal
 * These tests require an active ImGui context because Apply() calls
 * ImGui::GetStyle(). All tests are driven through Application::Run() with
 * TestHeadlessBackend (1 frame), which creates and destroys the ImGui context
 * around the run. Style assertions are captured inside the OnUi callback and
 * evaluated after Run() returns.
 *
 * Tests verify:
 * - WithTheme(Dracula) modifies ImGui::GetStyle() before the first OnUi call.
 * - Specific style fields match the expected Dracula token values.
 * - ThemeBuilder can override a single colour and produce a valid Theme.
 * - ThemeBuilder SetSpacing produces the expected item spacing in style.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-04
 * @version  0.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "../App/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Theme/Theme.hpp"
#include "ImFrame/Theme/ThemeBuilder.hpp"
#include "ImFrame/Theme/Themes/Dracula.hpp"
#include "ImFrame/Theme/Themes/Nord.hpp"

#include <imgui.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Theme::ColorRole;
using ImFrame::Theme::ColorToken;
using ImFrame::Theme::SpacingToken;
using ImFrame::Theme::ThemeBuilder;
using ImFrame::Tests::TestHeadlessBackend;
using Catch::Approx;

// ─── Apply ────────────────────────────────────────────────────────────────────

TEST_CASE("WithTheme(Dracula) sets WindowBg to backgroundPrimary", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    ImVec4 capturedWindowBg{};

    app.WithTheme(ImFrame::Themes::Dracula);
    app.OnUi([&] {
        capturedWindowBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    });

    auto result = app.Run();
    REQUIRE(result.has_value());

    // Dracula backgroundPrimary = #282A36 → (0.157, 0.165, 0.212, 1.0)
    REQUIRE(capturedWindowBg.x == Approx(ImFrame::Themes::Dracula.backgroundPrimary.r));
    REQUIRE(capturedWindowBg.y == Approx(ImFrame::Themes::Dracula.backgroundPrimary.g));
    REQUIRE(capturedWindowBg.z == Approx(ImFrame::Themes::Dracula.backgroundPrimary.b));
    REQUIRE(capturedWindowBg.w == Approx(ImFrame::Themes::Dracula.backgroundPrimary.a));
}

TEST_CASE("WithTheme(Dracula) sets FrameRounding to radius.Frame", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    float capturedRounding = -1.0f;

    app.WithTheme(ImFrame::Themes::Dracula);
    app.OnUi([&] {
        capturedRounding = ImGui::GetStyle().FrameRounding;
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(capturedRounding == Approx(ImFrame::Themes::Dracula.radius.Frame));
}

TEST_CASE("WithTheme(Dracula) sets ItemSpacing.x to spacing.ItemSpacingX", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    float capturedSpacingX = -1.0f;

    app.WithTheme(ImFrame::Themes::Dracula);
    app.OnUi([&] {
        capturedSpacingX = ImGui::GetStyle().ItemSpacing.x;
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(capturedSpacingX == Approx(ImFrame::Themes::Dracula.spacing.ItemSpacingX));
}

TEST_CASE("WithTheme applies before first OnUi call", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    // Verify the style is Dracula (not whatever the default was)
    bool styleWasApplied = false;

    app.WithTheme(ImFrame::Themes::Dracula);
    app.OnUi([&] {
        const ImVec4& bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        // Dracula background.r is ~0.157; ImGui's default dark theme background is
        // closer to 0.06. If the colour matches the Dracula token, Apply() ran first.
        styleWasApplied =
            (bg.x == Approx(ImFrame::Themes::Dracula.backgroundPrimary.r)) &&
            (bg.y == Approx(ImFrame::Themes::Dracula.backgroundPrimary.g));
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(styleWasApplied);
}

// ─── ThemeBuilder ─────────────────────────────────────────────────────────────

TEST_CASE("ThemeBuilder overrides one colour and Apply reflects the change", "[unit]") {
    // Start from Dracula, override backgroundPrimary to pure red
    auto custom = ThemeBuilder{ImFrame::Themes::Dracula}
                      .SetColor(ColorRole::BackgroundPrimary, ColorToken{1.0f, 0.0f, 0.0f, 1.0f})
                      .Build();

    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    ImVec4 capturedWindowBg{};

    app.WithTheme(custom);
    app.OnUi([&] {
        capturedWindowBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    });

    auto result = app.Run();
    REQUIRE(result.has_value());

    REQUIRE(capturedWindowBg.x == Approx(1.0f));
    REQUIRE(capturedWindowBg.y == Approx(0.0f));
    REQUIRE(capturedWindowBg.z == Approx(0.0f));
}

TEST_CASE("ThemeBuilder SetSpacing changes ItemSpacing in style", "[unit]") {
    constexpr float customX = 16.0f;
    constexpr float customY = 8.0f;

    auto custom = ThemeBuilder{ImFrame::Themes::Dracula}
                      .SetSpacing(SpacingToken{.ItemSpacingX = customX,
                                               .ItemSpacingY = customY,
                                               .WindowPaddingX = 8.0f,
                                               .WindowPaddingY = 8.0f,
                                               .FramePaddingX  = 4.0f,
                                               .FramePaddingY  = 3.0f,
                                               .IndentSpacing  = 21.0f})
                      .Build();

    auto backend = std::make_unique<TestHeadlessBackend>(1);
    Application app(std::move(backend));

    float capturedX = -1.0f;
    float capturedY = -1.0f;

    app.WithTheme(custom);
    app.OnUi([&] {
        capturedX = ImGui::GetStyle().ItemSpacing.x;
        capturedY = ImGui::GetStyle().ItemSpacing.y;
    });

    auto result = app.Run();
    REQUIRE(result.has_value());
    REQUIRE(capturedX == Approx(customX));
    REQUIRE(capturedY == Approx(customY));
}

TEST_CASE("ThemeBuilder Build() preserves unchanged fields from base", "[unit]") {
    // Override only accent, verify all other Dracula colours are unchanged
    auto custom = ThemeBuilder{ImFrame::Themes::Dracula}
                      .SetColor(ColorRole::AccentDefault, ColorToken{1.0f, 0.0f, 0.5f, 1.0f})
                      .Build();

    // backgroundPrimary should still be Dracula's value
    REQUIRE(custom.backgroundPrimary.r == Approx(ImFrame::Themes::Dracula.backgroundPrimary.r));
    REQUIRE(custom.backgroundPrimary.g == Approx(ImFrame::Themes::Dracula.backgroundPrimary.g));
    REQUIRE(custom.backgroundPrimary.b == Approx(ImFrame::Themes::Dracula.backgroundPrimary.b));

    // accentDefault should be the new value
    REQUIRE(custom.accentDefault.r == Approx(1.0f));
    REQUIRE(custom.accentDefault.g == Approx(0.0f));
    REQUIRE(custom.accentDefault.b == Approx(0.5f));
}
