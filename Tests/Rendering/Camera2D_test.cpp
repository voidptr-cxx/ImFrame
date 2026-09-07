/**
 * @file     Camera2D_test.cpp
 * @brief    Unit tests for Camera2D coordinate transforms, zoom clamping, and FitToRect
 *
 * Pure math tests — no ImGui context required.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-28
 * @version  2.0.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ImFrame/Rendering/Camera2D.hpp"

using namespace ImFrame;
using namespace ImFrame::Rendering;
using Catch::Matchers::WithinAbs;

static constexpr float kEps = 1e-4f;

// ─── CanvasToScreen (no rotation) ────────────────────────────────────────────

TEST_CASE("Camera2D::CanvasToScreen at identity maps origin to viewport origin", "[unit]") {
    Camera2D cam;
    const Widgets::Vec2 origin{100.f, 200.f};
    auto s = cam.CanvasToScreen({0.f, 0.f}, origin);
    REQUIRE_THAT(s.x, WithinAbs(100.f, kEps));
    REQUIRE_THAT(s.y, WithinAbs(200.f, kEps));
}

TEST_CASE("Camera2D::CanvasToScreen scales by zoom", "[unit]") {
    Camera2D cam;
    cam.SetZoom(2.0f);
    const Widgets::Vec2 origin{0.f, 0.f};
    auto s = cam.CanvasToScreen({50.f, 50.f}, origin);
    REQUIRE_THAT(s.x, WithinAbs(100.f, kEps));
    REQUIRE_THAT(s.y, WithinAbs(100.f, kEps));
}

TEST_CASE("Camera2D::CanvasToScreen offsets by camera position", "[unit]") {
    Camera2D cam;
    cam.SetPosition({10.f, 20.f}); // canvas point at top-left of viewport
    const Widgets::Vec2 origin{0.f, 0.f};
    // Canvas (10,20) should be at screen (0,0); canvas (20,30) → screen (10,10)
    auto s = cam.CanvasToScreen({20.f, 30.f}, origin);
    REQUIRE_THAT(s.x, WithinAbs(10.f, kEps));
    REQUIRE_THAT(s.y, WithinAbs(10.f, kEps));
}

// ─── ScreenToCanvas ───────────────────────────────────────────────────────────

TEST_CASE("Camera2D::ScreenToCanvas is the inverse of CanvasToScreen", "[unit]") {
    Camera2D cam;
    cam.SetPosition({-5.f, 3.f});
    cam.SetZoom(3.0f);
    const Widgets::Vec2 origin{50.f, 70.f};

    const Widgets::Vec2 canvas{100.f, -20.f};
    auto screen   = cam.CanvasToScreen(canvas, origin);
    auto roundTrip = cam.ScreenToCanvas(screen, origin);

    REQUIRE_THAT(roundTrip.x, WithinAbs(canvas.x, kEps));
    REQUIRE_THAT(roundTrip.y, WithinAbs(canvas.y, kEps));
}

// ─── Zoom clamping ────────────────────────────────────────────────────────────

TEST_CASE("Camera2D::SetZoom clamps to MinZoom", "[unit]") {
    Camera2D cam;
    cam.MinZoom(0.5f).SetZoom(0.01f);
    REQUIRE_THAT(cam.GetZoom(), WithinAbs(0.5f, kEps));
}

TEST_CASE("Camera2D::SetZoom clamps to MaxZoom", "[unit]") {
    Camera2D cam;
    cam.MaxZoom(5.0f).SetZoom(100.f);
    REQUIRE_THAT(cam.GetZoom(), WithinAbs(5.0f, kEps));
}

// ─── Reset ───────────────────────────────────────────────────────────────────

TEST_CASE("Camera2D::Reset restores identity state", "[unit]") {
    Camera2D cam;
    cam.SetPosition({10.f, 20.f}).SetZoom(4.0f).SetRotation(1.0f);
    cam.Reset();
    REQUIRE_THAT(cam.GetPosition().x, WithinAbs(0.f, kEps));
    REQUIRE_THAT(cam.GetPosition().y, WithinAbs(0.f, kEps));
    REQUIRE_THAT(cam.GetZoom(),       WithinAbs(1.f, kEps));
    REQUIRE_THAT(cam.GetRotation(),   WithinAbs(0.f, kEps));
}

// ─── FitToRect ────────────────────────────────────────────────────────────────

TEST_CASE("Camera2D::FitToRect centers the rectangle in the viewport", "[unit]") {
    Camera2D cam;
    // 100×100 rect at origin, 200×200 viewport → zoom ≈ 200/100 * 0.9 = 1.8
    cam.FitToRect({0.f, 0.f}, {100.f, 100.f}, {200.f, 200.f});

    // After FitToRect the rect center (50,50) should map to viewport center (100,100)
    const Widgets::Vec2 origin{0.f, 0.f};
    auto center = cam.CanvasToScreen({50.f, 50.f}, origin);
    REQUIRE_THAT(center.x, WithinAbs(100.f, 1.f)); // 1-pixel tolerance for floating-point
    REQUIRE_THAT(center.y, WithinAbs(100.f, 1.f));
}

TEST_CASE("Camera2D::FitToRect zoom is bounded by MaxZoom", "[unit]") {
    Camera2D cam;
    cam.MaxZoom(1.0f);
    cam.FitToRect({0.f, 0.f}, {10.f, 10.f}, {2000.f, 2000.f}); // would need zoom=180
    REQUIRE(cam.GetZoom() <= 1.0f + kEps);
}

TEST_CASE("Camera2D::FitToRect ignores zero-area rectangle", "[unit]") {
    Camera2D cam;
    cam.SetZoom(3.0f);
    cam.FitToRect({5.f, 5.f}, {5.f, 5.f}, {200.f, 200.f}); // zero area
    REQUIRE_THAT(cam.GetZoom(), WithinAbs(3.0f, kEps));       // unchanged
}
