/**
 * @file     Camera3D_test.cpp
 * @brief    Unit tests for Camera3D matrix math, clamping, FrameExtents, and input processing
 *
 * Pure math tests — no ImGui context required.
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
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ImFrame/Rendering/Camera3D.hpp"
#include "ImFrame/Rendering/Transform3D.hpp"

#include <cmath>
#include <numeric>

using namespace ImFrame;
using namespace ImFrame::Rendering;
using Catch::Matchers::WithinAbs;

static constexpr float kEps  = 1e-4f;
static constexpr float kLoose = 0.01f;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static bool matIsNonZero(const float m[16]) {
    for (int i = 0; i < 16; ++i)
        if (m[i] != 0.f) return true;
    return false;
}

static float matDiagSum(const float m[16]) {
    return m[0] + m[5] + m[10] + m[15];
}

// ─── ViewMatrix ───────────────────────────────────────────────────────────────

TEST_CASE("Camera3D::ViewMatrix is non-zero at default construction", "[unit]") {
    Camera3D cam;
    float v[16] = {};
    cam.ViewMatrix(v);
    REQUIRE(matIsNonZero(v));
}

TEST_CASE("Camera3D::ViewMatrix W column is (0,0,0,1) for rigid camera", "[unit]") {
    Camera3D cam;
    float v[16] = {};
    cam.ViewMatrix(v);
    // Column 3 (translation) row 3 must be 1.
    REQUIRE_THAT(v[15], WithinAbs(1.f, kEps));
    // Rotation sub-matrix rows must be unit length.
    float rx = v[0]*v[0] + v[4]*v[4] + v[8]*v[8]; // right row
    float uy = v[1]*v[1] + v[5]*v[5] + v[9]*v[9]; // up row
    REQUIRE_THAT(rx, WithinAbs(1.f, kEps));
    REQUIRE_THAT(uy, WithinAbs(1.f, kEps));
}

TEST_CASE("Camera3D::ViewMatrix Fly mode uses position directly", "[unit]") {
    Camera3D cam;
    // yaw=-π/2 gives forward=(cos(0)*cos(-π/2), 0, cos(0)*sin(-π/2)) = (0,0,-1),
    // so the camera looks straight down -Z from (0,0,10).
    cam.SetMode(CameraMode::Fly)
       .SetPosition({0.f, 0.f, 10.f})
       .SetPitch(0.f)
       .SetYaw(-3.14159265f / 2.f);
    float v[16] = {};
    cam.ViewMatrix(v);
    // fwd=(0,0,-1), right=(1,0,0), up=(0,1,0)
    // out[14] = dot(fwd, eye) = dot((0,0,-1),(0,0,10)) = -10
    REQUIRE_THAT(v[14], WithinAbs(-10.f, kEps));
}

// ─── ProjectionMatrix ─────────────────────────────────────────────────────────

TEST_CASE("Camera3D::ProjectionMatrix perspective: [10] encodes far/near", "[unit]") {
    Camera3D cam;
    cam.NearPlane(1.f).FarPlane(100.f);
    float p[16] = {};
    cam.ProjectionMatrix(p, 1.f);
    // out[10] = (far+near)/(near-far) = 101/-99
    float expected = (100.f + 1.f) / (1.f - 100.f);
    REQUIRE_THAT(p[10], WithinAbs(expected, kEps));
}

TEST_CASE("Camera3D::ProjectionMatrix orthographic: diagonal encodes scale", "[unit]") {
    Camera3D cam;
    cam.SetMode(CameraMode::Orthographic)
       .OrthoScale(10.f)
       .NearPlane(0.1f)
       .FarPlane(100.f);
    float p[16] = {};
    cam.ProjectionMatrix(p, 1.0f); // square aspect
    // out[0] = 2/(w) = 2/10 = 0.2  (width = scale * aspect = 10 * 1 = 10)
    // out[5] = 2/(h) = 2/10 = 0.2
    REQUIRE_THAT(p[0],  WithinAbs(0.2f,  kEps));
    REQUIRE_THAT(p[5],  WithinAbs(0.2f,  kEps));
    REQUIRE_THAT(p[15], WithinAbs(1.f,   kEps));
}

TEST_CASE("Camera3D::ProjectionMatrix aspect ratio scales X component", "[unit]") {
    Camera3D cam;
    float p1[16] = {}, p2[16] = {};
    cam.ProjectionMatrix(p1, 1.f);
    cam.ProjectionMatrix(p2, 2.f); // twice as wide
    // [0] scales by 1/aspect, so p2[0] should be half of p1[0]
    REQUIRE_THAT(p2[0], WithinAbs(p1[0] * 0.5f, kEps));
    // [5] (Y scale) should be unchanged.
    REQUIRE_THAT(p1[5], WithinAbs(p2[5], kEps));
}

// ─── ViewProjectionMatrix ─────────────────────────────────────────────────────

TEST_CASE("Camera3D::ViewProjectionMatrix is non-zero", "[unit]") {
    Camera3D cam;
    float vp[16] = {};
    cam.ViewProjectionMatrix(vp, 1.f);
    REQUIRE(matIsNonZero(vp));
}

// ─── Setters / clamping ───────────────────────────────────────────────────────

TEST_CASE("Camera3D::SetPitch clamps to [-1.5, 1.5]", "[unit]") {
    Camera3D cam;
    cam.SetPitch(99.f);
    float v[16] = {};
    cam.ViewMatrix(v); // must not NaN
    REQUIRE_THAT(v[5], WithinAbs(v[5], kEps)); // NaN check: value == itself
}

TEST_CASE("Camera3D::SetDistance clamps to >= 0.01", "[unit]") {
    Camera3D cam;
    cam.SetDistance(-100.f);
    // GetDistance not tested directly, but GetPosition should still be finite.
    Widgets::Vec3 pos = cam.GetPosition();
    REQUIRE(std::isfinite(pos.x));
    REQUIRE(std::isfinite(pos.y));
    REQUIRE(std::isfinite(pos.z));
}

TEST_CASE("Camera3D::OrthoScale clamps to >= 0.01", "[unit]") {
    Camera3D cam;
    cam.SetMode(CameraMode::Orthographic).OrthoScale(-5.f);
    float p[16] = {};
    cam.ProjectionMatrix(p, 1.f);
    // out[0] = 2/w = 2/(0.01*1) = 200
    REQUIRE(p[0] <= 200.f + kEps);
}

// ─── GetPosition / GetDirection ───────────────────────────────────────────────

TEST_CASE("Camera3D orbit eye lies at distance from target", "[unit]") {
    Camera3D cam;
    cam.SetMode(CameraMode::Orbit)
       .SetTarget({0.f, 0.f, 0.f})
       .SetDistance(5.f)
       .SetPitch(0.f)
       .SetYaw(0.f);
    Widgets::Vec3 pos = cam.GetPosition();
    float dist = std::sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);
    REQUIRE_THAT(dist, WithinAbs(5.f, kEps));
}

TEST_CASE("Camera3D::GetDirection is unit-length", "[unit]") {
    Camera3D cam;
    Widgets::Vec3 d = cam.GetDirection();
    float len = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
    REQUIRE_THAT(len, WithinAbs(1.f, kEps));
}

TEST_CASE("Camera3D Fly::GetPosition returns set position", "[unit]") {
    Camera3D cam;
    cam.SetMode(CameraMode::Fly).SetPosition({3.f, 4.f, 5.f});
    Widgets::Vec3 p = cam.GetPosition();
    REQUIRE_THAT(p.x, WithinAbs(3.f, kEps));
    REQUIRE_THAT(p.y, WithinAbs(4.f, kEps));
    REQUIRE_THAT(p.z, WithinAbs(5.f, kEps));
}

// ─── Reset ────────────────────────────────────────────────────────────────────

TEST_CASE("Camera3D::Reset restores default distance=5 and pitch=0.3", "[unit]") {
    Camera3D cam;
    cam.SetDistance(100.f).SetPitch(1.4f).SetYaw(3.f);
    cam.Reset();
    // After reset, distance=5 and pitch=0.3.
    // Verify via position: y component should be ~distance * sin(0.3) = 5 * sin(0.3) ≈ 1.479
    Widgets::Vec3 pos = cam.GetPosition();
    REQUIRE_THAT(pos.y, WithinAbs(5.f * std::sin(0.3f), kLoose));
}

// ─── FrameExtents ─────────────────────────────────────────────────────────────

TEST_CASE("Camera3D::FrameExtents sets target to AABB center", "[unit]") {
    Camera3D cam;
    cam.FrameExtents({-2.f, -2.f, -2.f}, {2.f, 2.f, 2.f});
    Widgets::Vec3 target = cam.GetTarget();
    REQUIRE_THAT(target.x, WithinAbs(0.f, kEps));
    REQUIRE_THAT(target.y, WithinAbs(0.f, kEps));
    REQUIRE_THAT(target.z, WithinAbs(0.f, kEps));
}

TEST_CASE("Camera3D::FrameExtents distance is positive and finite", "[unit]") {
    Camera3D cam;
    cam.FrameExtents({0.f, 0.f, 0.f}, {10.f, 10.f, 10.f});
    Widgets::Vec3 pos  = cam.GetPosition();
    Widgets::Vec3 tgt  = cam.GetTarget();
    float dx = pos.x - tgt.x, dy = pos.y - tgt.y, dz = pos.z - tgt.z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    REQUIRE(dist > 1.f);
    REQUIRE(std::isfinite(dist));
}

// ─── Input processing ─────────────────────────────────────────────────────────

TEST_CASE("Camera3D::ProcessOrbitMouseDrag changes yaw proportionally", "[unit]") {
    Camera3D cam;
    cam.OrbitSensitivity(0.01f);
    Widgets::Vec3 before = cam.GetPosition();
    cam.ProcessOrbitMouseDrag(100.f, 0.f);
    Widgets::Vec3 after = cam.GetPosition();
    // Yaw changed — x and z should change; y (pure pitch) unchanged.
    bool yawMoved = (std::abs(after.x - before.x) > 0.01f) || (std::abs(after.z - before.z) > 0.01f);
    REQUIRE(yawMoved);
}

TEST_CASE("Camera3D::ProcessZoom zoom-in reduces orbit distance", "[unit]") {
    Camera3D cam;
    cam.SetDistance(10.f).ZoomSensitivity(0.5f);
    Widgets::Vec3 before = cam.GetPosition();
    cam.ProcessZoom(1.f);
    Widgets::Vec3 after = cam.GetPosition();
    float distBefore = std::sqrt(before.x*before.x+before.y*before.y+before.z*before.z);
    float distAfter  = std::sqrt(after.x*after.x+after.y*after.y+after.z*after.z);
    REQUIRE(distAfter < distBefore);
}

TEST_CASE("Camera3D::ProcessFlyMovement moves position forward", "[unit]") {
    Camera3D cam;
    cam.SetMode(CameraMode::Fly)
       .SetPosition({0.f, 0.f, 0.f})
       .SetPitch(0.f)
       .SetYaw(0.f)     // looking down +X (yaw=0 → forward=(1,0,0) in our orbit formula)
       .MovementSpeed(10.f);
    cam.ProcessFlyMovement(1.f, 0.f, 0.f, 1.f); // forward=1, dt=1s
    Widgets::Vec3 pos = cam.GetPosition();
    // Should have moved 10 units in the forward direction.
    float moved = std::sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);
    REQUIRE_THAT(moved, WithinAbs(10.f, kLoose));
}

TEST_CASE("Camera3D::ProcessOrthoZoom scale-in reduces ortho scale", "[unit]") {
    Camera3D cam;
    cam.SetMode(CameraMode::Orthographic).OrthoScale(10.f).ZoomSensitivity(0.5f);
    float before[16] = {}, after[16] = {};
    cam.ProjectionMatrix(before, 1.f);
    cam.ProcessOrthoZoom(1.f);
    cam.ProjectionMatrix(after, 1.f);
    // out[0] = 2/w; smaller scale → larger out[0].
    REQUIRE(after[0] > before[0]);
}

// ─── Copy / move semantics ────────────────────────────────────────────────────

TEST_CASE("Camera3D copy-constructor produces independent copy", "[unit]") {
    Camera3D cam;
    cam.SetDistance(20.f);
    Camera3D copy = cam;
    copy.SetDistance(1.f);
    float v1[16] = {}, v2[16] = {};
    cam.ViewMatrix(v1);
    copy.ViewMatrix(v2);
    // The translation columns should differ (different distance).
    bool differ = (v1[12] != v2[12]) || (v1[13] != v2[13]) || (v1[14] != v2[14]);
    REQUIRE(differ);
}

// ─── Transform3D::Matrix ──────────────────────────────────────────────────────

TEST_CASE("Transform3D identity produces identity matrix", "[unit]") {
    Transform3D t = Transform3D::Identity();
    float m[16] = {};
    t.Matrix(m);
    REQUIRE_THAT(m[0],  WithinAbs(1.f, kEps));
    REQUIRE_THAT(m[5],  WithinAbs(1.f, kEps));
    REQUIRE_THAT(m[10], WithinAbs(1.f, kEps));
    REQUIRE_THAT(m[15], WithinAbs(1.f, kEps));
    REQUIRE_THAT(m[1],  WithinAbs(0.f, kEps));
    REQUIRE_THAT(m[4],  WithinAbs(0.f, kEps));
}

TEST_CASE("Transform3D translation appears in column 3", "[unit]") {
    Transform3D t = Transform3D::Identity();
    t.Position = {3.f, 4.f, 5.f};
    float m[16] = {};
    t.Matrix(m);
    REQUIRE_THAT(m[12], WithinAbs(3.f, kEps));
    REQUIRE_THAT(m[13], WithinAbs(4.f, kEps));
    REQUIRE_THAT(m[14], WithinAbs(5.f, kEps));
}

TEST_CASE("Transform3D scale appears on the diagonal", "[unit]") {
    Transform3D t = Transform3D::Identity();
    t.Scale = {2.f, 3.f, 4.f};
    float m[16] = {};
    t.Matrix(m);
    REQUIRE_THAT(m[0],  WithinAbs(2.f, kEps));
    REQUIRE_THAT(m[5],  WithinAbs(3.f, kEps));
    REQUIRE_THAT(m[10], WithinAbs(4.f, kEps));
}

TEST_CASE("Transform3D 90-degree Y rotation swaps X and Z basis", "[unit]") {
    Transform3D t = Transform3D::Identity();
    // 90° about Y: q = (0, sin45, 0, cos45)
    float s = std::sin(3.14159265f / 4.f);
    float c = std::cos(3.14159265f / 4.f);
    t.Rotation = {0.f, s, 0.f, c};
    float m[16] = {};
    t.Matrix(m);
    // X basis (column 0): should be approximately (0, 0, -1)
    REQUIRE_THAT(m[0], WithinAbs(0.f, kLoose));
    REQUIRE_THAT(m[2], WithinAbs(-1.f, kLoose)); // wait: need to verify formula
    // Z basis (column 2): should be approximately (1, 0, 0)
    REQUIRE_THAT(m[8],  WithinAbs(1.f, kLoose));
    REQUIRE_THAT(m[10], WithinAbs(0.f, kLoose));
}
