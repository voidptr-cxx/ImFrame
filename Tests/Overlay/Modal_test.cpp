/**
 * @file     Modal_test.cpp
 * @brief    Unit tests for Modal, PopupScope, and ConfirmModal
 *
 * @internal
 * PopupScope construction/destruction tests need no ImGui context when the scope
 * is created with `open = false` (no EndPopup call).  Tests that require
 * BeginPopupModal use TestHeadlessBackend(1) + Application::Run().
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
#include "ImFrame/Overlay/Modal.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using namespace ImFrame::Overlay;

// ─── PopupScope ───────────────────────────────────────────────────────────────

TEST_CASE("PopupScope - false scope evaluates to false", "[overlay][unit]") {
    // No ImGui context needed: _open=false means the destructor will not call EndPopupModal.
    PopupScope s(false);
    CHECK(!static_cast<bool>(s));
} // destructor: _open=false → EndPopupModal is NOT called

TEST_CASE("PopupScope - move leaves original inactive", "[overlay][unit]") {
    PopupScope s1(false);
    PopupScope s2(std::move(s1));

    // Both report false (s1 was moved from; s2 was constructed with false)
    CHECK(!static_cast<bool>(s1)); // NOLINT — intentional moved-from read
    CHECK(!static_cast<bool>(s2));
}

// ─── Modal ────────────────────────────────────────────────────────────────────

TEST_CASE("Modal - Open() + Begin() opens the popup in the same frame", "[overlay][unit]") {
    Application app(std::make_unique<TestHeadlessBackend>(1));

    Modal modal("TestModal##open_test");
    bool  opened = false;

    app.OnUi([&] {
        modal.Open();
        if (auto scope = modal.Begin()) {
            opened = true;
            modal.Close();
        }
    });

    REQUIRE(app.Run().has_value());
    CHECK(opened);
}

TEST_CASE("Modal - Begin() without Open() does not open the popup", "[overlay][unit]") {
    Application app(std::make_unique<TestHeadlessBackend>(1));

    Modal modal("TestModal##no_open");
    bool  opened = false;

    app.OnUi([&] {
        if (auto scope = modal.Begin()) {
            opened = true;
        }
    });

    REQUIRE(app.Run().has_value());
    CHECK(!opened);
}

TEST_CASE("Modal - Size() and NoClose() do not crash", "[overlay][unit]") {
    Application app(std::make_unique<TestHeadlessBackend>(1));

    Modal modal("SizedModal##sized");
    modal.Size(400.0f, 300.0f).NoClose(true);
    modal.Open();

    app.OnUi([&] {
        if (auto scope = modal.Begin()) {
            modal.Close();
        }
    });

    REQUIRE(app.Run().has_value());
}

// ─── ConfirmModal ─────────────────────────────────────────────────────────────

TEST_CASE("ConfirmModal - Show() without Open() does not crash", "[overlay][unit]") {
    Application app(std::make_unique<TestHeadlessBackend>(1));

    ConfirmModal confirm("##confirm_no_open");
    confirm.Title("Delete").Message("Are you sure?");

    app.OnUi([&] {
        confirm.Show(); // not triggered — should be a no-op
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("ConfirmModal - Open() + Show() renders without crash", "[overlay][unit]") {
    Application app(std::make_unique<TestHeadlessBackend>(1));

    ConfirmModal confirm("##confirm_open");
    confirm.Title("Confirm Action").Message("Proceed?");
    confirm.OnResult([](bool) {});
    confirm.Open();

    app.OnUi([&] {
        confirm.Show();
    });

    REQUIRE(app.Run().has_value());
}

TEST_CASE("ConfirmModal - fluent builder returns self for chaining", "[overlay][unit]") {
    // Verifies all builder methods compile and return ConfirmModal&
    ConfirmModal confirm("##chain_test");
    auto& ref = confirm
        .Title("T")
        .Message("M")
        .ConfirmLabel("Yes")
        .CancelLabel("No")
        .OnResult([](bool) {});

    CHECK(std::addressof(ref) == std::addressof(confirm));
}
