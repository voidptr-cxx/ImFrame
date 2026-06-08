/**
 * @file     Toast_test.cpp
 * @brief    Unit tests for ToastManager — queue management, promotion, and opacity decay
 *
 * @internal
 * Uses TestHeadlessBackend(1) + Application::Run() for tests that need a live
 * ImGui frame (Render calls).  Pure-logic tests (Add / ActiveCount / QueuedCount)
 * do not require a frame and use the singleton directly.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "../App/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Overlay/Toast.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using namespace ImFrame::Overlay;

// Helper: configure a fresh manager before each test
static void ResetManager(int maxVisible = 3, float duration = 2.0f,
                          float fadeIn = 0.1f, float fadeOut = 0.1f) {
    ToastManager::Instance().Clear();
    ToastManager::Instance().Configure({
        .maxVisible      = maxVisible,
        .defaultDuration = duration,
        .fadeInDuration  = fadeIn,
        .fadeOutDuration = fadeOut,
    });
}

// ─── Queue management (no ImGui context needed) ───────────────────────────────

TEST_CASE("ToastManager - at most maxVisible toasts are active", "[overlay][unit]") {
    ResetManager(3, 5.0f);

    ToastInfo("T1");
    ToastInfo("T2");
    ToastInfo("T3");
    ToastInfo("T4");
    ToastInfo("T5");

    CHECK(ToastManager::Instance().ActiveCount() == 3);
    CHECK(ToastManager::Instance().QueuedCount() == 2);
}

TEST_CASE("ToastManager - single toast below maxVisible stays active", "[overlay][unit]") {
    ResetManager(3, 5.0f);

    ToastSuccess("Only one");

    CHECK(ToastManager::Instance().ActiveCount() == 1);
    CHECK(ToastManager::Instance().QueuedCount() == 0);
}

TEST_CASE("ToastManager - type variety queues correctly", "[overlay][unit]") {
    ResetManager(2, 5.0f);

    ToastInfo("I");
    ToastSuccess("S");
    ToastWarning("W"); // queued
    ToastError("E");   // queued

    CHECK(ToastManager::Instance().ActiveCount() == 2);
    CHECK(ToastManager::Instance().QueuedCount() == 2);
}

// ─── Queue promotion (requires ImGui frame for Render) ────────────────────────

TEST_CASE("ToastManager - queued toast promotes when active expires", "[overlay][unit]") {
    ResetManager(2, 0.5f, 0.05f, 0.05f);

    ToastInfo("A");
    ToastInfo("B");
    ToastInfo("C"); // queued

    REQUIRE(ToastManager::Instance().ActiveCount() == 2);
    REQUIRE(ToastManager::Instance().QueuedCount() == 1);

    Application app(std::make_unique<TestHeadlessBackend>(1));

    bool promoted = false;
    app.OnUi([&] {
        // Large dt → A and B expire, C is promoted
        ToastManager::Instance().Render(10.0f);
        promoted = (ToastManager::Instance().ActiveCount() == 1 &&
                    ToastManager::Instance().QueuedCount() == 0);
    });

    REQUIRE(app.Run().has_value());
    CHECK(promoted);
}

// ─── Opacity / expiry ─────────────────────────────────────────────────────────

TEST_CASE("ToastManager - toast is removed after duration + fade-out", "[overlay][unit]") {
    ResetManager(1, 0.5f, 0.05f, 0.05f);

    ToastInfo("X");
    REQUIRE(ToastManager::Instance().ActiveCount() == 1);

    Application app(std::make_unique<TestHeadlessBackend>(1));

    bool expired = false;
    app.OnUi([&] {
        // 10s >> 0.5s duration + fades — toast should be removed
        ToastManager::Instance().Render(10.0f);
        expired = (ToastManager::Instance().ActiveCount() == 0);
    });

    REQUIRE(app.Run().has_value());
    CHECK(expired);
}

TEST_CASE("ToastManager - all 5 toasts eventually expire", "[overlay][unit]") {
    ResetManager(3, 0.5f, 0.05f, 0.05f);

    for (int i = 0; i < 5; ++i) {
        ToastInfo("T" + std::to_string(i));
    }

    Application app(std::make_unique<TestHeadlessBackend>(1));

    bool allGone = false;
    app.OnUi([&] {
        // First render expires the 3 active; second expires the 2 promoted
        ToastManager::Instance().Render(10.0f);
        ToastManager::Instance().Render(10.0f);
        allGone = (ToastManager::Instance().ActiveCount() == 0 &&
                   ToastManager::Instance().QueuedCount() == 0);
    });

    REQUIRE(app.Run().has_value());
    CHECK(allGone);
}
