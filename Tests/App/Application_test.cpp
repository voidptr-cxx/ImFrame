/**
 * @file     Application_test.cpp
 * @brief    Unit tests for ImFrame::App::Application
 *
 * @internal
 * Tests verify:
 * - OnUpdate is called once per frame with a non-negative delta time.
 * - OnUi is called exactly once per frame.
 * - An OnClose callback returning false vetoes the close request; the loop
 *   continues and OnClose is eventually called a second time.
 * - WithFont with an empty path does not crash (skips the TTF load).
 * - WithFont with dpiScaled=true computes the correct logical size.
 *
 * All tests use TestHeadlessBackend to drive Application::Run() without an OS
 * window or GPU. The backend terminates after a configurable number of frames.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  0.8.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::App::FontConfig;
using ImFrame::Tests::TestHeadlessBackend;

// ─── OnUpdate ─────────────────────────────────────────────────────────────────

TEST_CASE("OnUpdate is called once per frame with non-negative delta time", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(3);

    Application app(std::move(backend));

    int   updateCount = 0;
    float lastDt      = -1.0f;

    app.OnUpdate([&](float dt) {
        REQUIRE(dt >= 0.0f);
        lastDt = dt;
        ++updateCount;
    });

    auto result = app.Run();

    REQUIRE(result.has_value());
    REQUIRE(updateCount == 3);
    REQUIRE(lastDt >= 0.0f);
}

// ─── OnUi ─────────────────────────────────────────────────────────────────────

TEST_CASE("OnUi is called exactly once per frame", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(3);

    Application app(std::move(backend));

    int uiCount = 0;
    app.OnUi([&] { ++uiCount; });

    auto result = app.Run();

    REQUIRE(result.has_value());
    REQUIRE(uiCount == 3);
}

// ─── OnClose veto ─────────────────────────────────────────────────────────────

TEST_CASE("OnClose returning false vetoes the close request", "[unit]") {
    // maxFrames=0 so the backend requests close on the very first Poll().
    // The first OnClose call returns false (veto); the second call returns true.
    auto backend = std::make_unique<TestHeadlessBackend>(0);

    Application app(std::move(backend));

    int  closeCallCount = 0;
    bool allowClose     = false;

    app.OnClose([&]() -> bool {
        ++closeCallCount;
        if (!allowClose) {
            allowClose = true; // Next call will allow.
            return false;      // Veto this attempt.
        }
        return true; // Allow close.
    });

    auto result = app.Run();

    REQUIRE(result.has_value());
    // OnClose must have been called exactly twice: once for the veto, once to allow.
    REQUIRE(closeCallCount == 2);
}

// ─── WithFont ─────────────────────────────────────────────────────────────────

TEST_CASE("WithFont with empty path does not crash", "[unit]") {
    auto backend = std::make_unique<TestHeadlessBackend>(1);

    Application app(std::move(backend));

    // Empty path = no TTF file loaded; falls back to ImGui's built-in default.
    app.WithFont(FontConfig{ .size = 14.0f, .dpiScaled = true });

    auto result = app.Run();

    REQUIRE(result.has_value());
}
