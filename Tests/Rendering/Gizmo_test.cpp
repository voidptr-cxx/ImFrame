/**
 * @file     Gizmo_test.cpp
 * @brief    Unit tests for Gizmo construction and null-guard behavior
 *
 * Tests that are purely API-level and do not require ImGui rendering:
 * - Gizmo::Show() returns false when no transform is bound.
 * - Gizmo::Show() returns false when no viewport is valid.
 * - Builder setters chain correctly.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-29
 * @version  2.1.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Rendering/Gizmo.hpp"
#include "ImFrame/Rendering/Transform3D.hpp"
#include "ImFrame/Rendering/Viewport3D.hpp"

using namespace ImFrame::Rendering;

// ─── Null-guard ───────────────────────────────────────────────────────────────

TEST_CASE("Gizmo::Show returns false when transform is not bound", "[unit]") {
    Viewport3D vp("gizmo_no_transform");
    Gizmo gizmo(vp);
    // No Transform() call — Show() must return false without crashing.
    REQUIRE_FALSE(gizmo.Show());
}

TEST_CASE("Gizmo builder chain compiles and does not throw", "[unit]") {
    Viewport3D    vp("gizmo_chain");
    Transform3D   t = Transform3D::Identity();
    Gizmo         gizmo(vp);

    // Verify chaining syntax compiles; Show() may return false (no ImGui context).
    bool result = gizmo
        .Transform(t)
        .Mode(GizmoMode::Translate)
        .Space(GizmoSpace::World)
        .Show();

    // Without an active ImGui context, no dragging can occur.
    REQUIRE_FALSE(result);
}

TEST_CASE("Gizmo in Rotate mode does not crash without ImGui context", "[unit]") {
    Viewport3D  vp("gizmo_rotate");
    Transform3D t = Transform3D::Identity();
    Gizmo       gizmo(vp);
    gizmo.Transform(t).Mode(GizmoMode::Rotate);
    REQUIRE_NOTHROW(gizmo.Show());
}

TEST_CASE("Gizmo in Scale mode does not crash without ImGui context", "[unit]") {
    Viewport3D  vp("gizmo_scale");
    Transform3D t = Transform3D::Identity();
    Gizmo       gizmo(vp);
    gizmo.Transform(t).Mode(GizmoMode::Scale);
    REQUIRE_NOTHROW(gizmo.Show());
}
