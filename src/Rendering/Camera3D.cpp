/**
 * @file     Camera3D.cpp
 * @brief    Camera3D view/projection matrix computation and input processing
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

#include "ImFrame/Rendering/Camera3D.hpp"

#include <algorithm>
#include <cmath>

using Vec3 = ImFrame::Widgets::Vec3;

// ─── Internal math helpers ────────────────────────────────────────────────────

namespace {

float dot3(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross3(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 norm3(Vec3 v) noexcept {
    float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return (l > 1e-8f) ? Vec3{v.x / l, v.y / l, v.z / l} : Vec3{0.f, 0.f, 1.f};
}

Vec3 sub3(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

// Column-major LookAt. Rows: [right, up, -forward, 0], column 3 = translations.
void lookAt(Vec3 eye, Vec3 center, Vec3 worldUp, float out[16]) noexcept {
    Vec3 f = norm3(sub3(center, eye)); // forward
    Vec3 r = norm3(cross3(f, worldUp)); // right
    Vec3 u = cross3(r, f);             // recalculated up

    out[0]  = r.x;   out[4]  = r.y;   out[8]   = r.z;    out[12] = -dot3(r, eye);
    out[1]  = u.x;   out[5]  = u.y;   out[9]   = u.z;    out[13] = -dot3(u, eye);
    out[2]  = -f.x;  out[6]  = -f.y;  out[10]  = -f.z;   out[14] =  dot3(f, eye);
    out[3]  = 0.f;   out[7]  = 0.f;   out[11]  = 0.f;    out[15] =  1.f;
}

// Right-handed perspective projection, NDC depth in [-1, 1] (OpenGL convention).
void perspective(float fov, float aspect, float nearP, float farP, float out[16]) noexcept {
    float f = 1.f / std::tan(fov * 0.5f);
    float A = (farP + nearP) / (nearP - farP);
    float B = 2.f * farP * nearP / (nearP - farP);
    for (int i = 0; i < 16; ++i) out[i] = 0.f;
    out[0]  = f / aspect;
    out[5]  = f;
    out[10] = A;  out[11] = -1.f;
    out[14] = B;
}

// Symmetric right-handed orthographic projection, NDC depth in [-1, 1].
void ortho(float w, float h, float nearP, float farP, float out[16]) noexcept {
    for (int i = 0; i < 16; ++i) out[i] = 0.f;
    out[0]  = 2.f / w;
    out[5]  = 2.f / h;
    out[10] = -2.f / (farP - nearP);
    out[14] = -(farP + nearP) / (farP - nearP);
    out[15] = 1.f;
}

// Column-major 4x4 matrix multiply: out = a * b.
void matMul4(const float a[16], const float b[16], float out[16]) noexcept {
    for (int j = 0; j < 4; ++j)
        for (int i = 0; i < 4; ++i) {
            float s = 0.f;
            for (int k = 0; k < 4; ++k)
                s += a[k * 4 + i] * b[j * 4 + k];
            out[j * 4 + i] = s;
        }
}

} // anonymous namespace

// ─── Impl ─────────────────────────────────────────────────────────────────────

namespace ImFrame::Rendering {

struct Camera3D::Impl {
    CameraMode mode      = CameraMode::Orbit;
    float      pitch     = 0.3f;    // radians above horizon
    float      yaw       = 0.f;     // horizontal radians
    float      nearPlane = 0.1f;
    float      farPlane  = 1000.f;
    float      fov       = 1.0472f; // π/3 ≈ 60°

    // Orbit / Orthographic state
    Vec3  target     = {0.f, 0.f, 0.f};
    float distance   = 5.f;
    float orthoScale = 10.f;

    // Fly state
    Vec3 position = {0.f, 0.f, 5.f};

    // Sensitivities
    float orbitSens = 0.01f;
    float panSens   = 0.005f;
    float zoomSens  = 0.5f;
    float moveSpeed = 5.f;
    float lookSens  = 0.003f;
    float speedMult = 3.f;

    [[nodiscard]] Vec3 orbitEye() const noexcept {
        return {
            target.x + distance * std::cos(pitch) * std::cos(yaw),
            target.y + distance * std::sin(pitch),
            target.z + distance * std::cos(pitch) * std::sin(yaw)
        };
    }

    [[nodiscard]] Vec3 flyForward() const noexcept {
        return {
            std::cos(pitch) * std::cos(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::sin(yaw)
        };
    }
};

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Camera3D::Camera3D()  : _impl(std::make_unique<Impl>()) {}
Camera3D::~Camera3D() noexcept = default;

Camera3D::Camera3D(const Camera3D& o)  : _impl(std::make_unique<Impl>(*o._impl)) {}
Camera3D& Camera3D::operator=(const Camera3D& o) {
    if (this != &o) *_impl = *o._impl;
    return *this;
}
Camera3D::Camera3D(Camera3D&&) noexcept            = default;
Camera3D& Camera3D::operator=(Camera3D&&) noexcept = default;

// ─── Matrix output ────────────────────────────────────────────────────────────

void Camera3D::ViewMatrix(float out[16]) const noexcept {
    Vec3 eye, center;
    if (_impl->mode == CameraMode::Fly) {
        eye    = _impl->position;
        Vec3 d = _impl->flyForward();
        center = {eye.x + d.x, eye.y + d.y, eye.z + d.z};
    } else {
        eye    = _impl->orbitEye();
        center = _impl->target;
    }
    lookAt(eye, center, {0.f, 1.f, 0.f}, out);
}

void Camera3D::ProjectionMatrix(float out[16], float aspectRatio) const noexcept {
    if (_impl->mode == CameraMode::Orthographic) {
        float h = _impl->orthoScale;
        ortho(h * aspectRatio, h, _impl->nearPlane, _impl->farPlane, out);
    } else {
        perspective(_impl->fov, aspectRatio, _impl->nearPlane, _impl->farPlane, out);
    }
}

void Camera3D::ViewProjectionMatrix(float out[16], float aspectRatio) const noexcept {
    float v[16], p[16];
    ViewMatrix(v);
    ProjectionMatrix(p, aspectRatio);
    matMul4(p, v, out);
}

// ─── Configuration setters ────────────────────────────────────────────────────

Camera3D& Camera3D::SetMode(CameraMode m) noexcept       { _impl->mode = m; return *this; }
Camera3D& Camera3D::SetTarget(Vec3 t) noexcept            { _impl->target = t; return *this; }
Camera3D& Camera3D::SetDistance(float d) noexcept         { _impl->distance = std::max(d, 0.01f); return *this; }
Camera3D& Camera3D::SetPitch(float p) noexcept            { _impl->pitch = std::clamp(p, -1.5f, 1.5f); return *this; }
Camera3D& Camera3D::SetYaw(float y) noexcept              { _impl->yaw = y; return *this; }
Camera3D& Camera3D::SetPosition(Vec3 pos) noexcept        { _impl->position = pos; return *this; }
Camera3D& Camera3D::OrthoScale(float s) noexcept          { _impl->orthoScale = std::max(s, 0.01f); return *this; }
Camera3D& Camera3D::NearPlane(float n) noexcept           { _impl->nearPlane = std::max(n, 0.0001f); return *this; }
Camera3D& Camera3D::FarPlane(float f) noexcept            { _impl->farPlane = f; return *this; }
Camera3D& Camera3D::FieldOfView(float fov) noexcept       { _impl->fov = fov; return *this; }
Camera3D& Camera3D::OrbitSensitivity(float s) noexcept    { _impl->orbitSens = s; return *this; }
Camera3D& Camera3D::PanSensitivity(float s) noexcept      { _impl->panSens = s; return *this; }
Camera3D& Camera3D::ZoomSensitivity(float s) noexcept     { _impl->zoomSens = s; return *this; }
Camera3D& Camera3D::MovementSpeed(float s) noexcept       { _impl->moveSpeed = s; return *this; }
Camera3D& Camera3D::LookSensitivity(float s) noexcept     { _impl->lookSens = s; return *this; }
Camera3D& Camera3D::SpeedMultiplier(float m) noexcept     { _impl->speedMult = m; return *this; }

// ─── Getters ──────────────────────────────────────────────────────────────────

Vec3 Camera3D::GetPosition() const noexcept {
    return (_impl->mode == CameraMode::Fly) ? _impl->position : _impl->orbitEye();
}

Vec3 Camera3D::GetDirection() const noexcept {
    if (_impl->mode == CameraMode::Fly) return norm3(_impl->flyForward());
    return norm3(sub3(_impl->target, _impl->orbitEye()));
}

Vec3      Camera3D::GetTarget()      const noexcept { return _impl->target; }
float     Camera3D::GetNearPlane()   const noexcept { return _impl->nearPlane; }
float     Camera3D::GetFarPlane()    const noexcept { return _impl->farPlane; }
float     Camera3D::GetFieldOfView() const noexcept { return _impl->fov; }
float     Camera3D::GetOrthoScale()  const noexcept { return _impl->orthoScale; }
float     Camera3D::GetDistance()    const noexcept { return _impl->distance; }
CameraMode Camera3D::GetMode()       const noexcept { return _impl->mode; }
float     Camera3D::GetSpeedMultiplier() const noexcept { return _impl->speedMult; }

// ─── Utility ──────────────────────────────────────────────────────────────────

void Camera3D::Reset() noexcept {
    _impl->target     = {0.f, 0.f, 0.f};
    _impl->distance   = 5.f;
    _impl->pitch      = 0.3f;
    _impl->yaw        = 0.f;
    _impl->position   = {0.f, 0.f, 5.f};
    _impl->orthoScale = 10.f;
}

void Camera3D::FrameExtents(Vec3 mn, Vec3 mx) noexcept {
    _impl->target = {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f};
    float dx   = mx.x - mn.x, dy = mx.y - mn.y, dz = mx.z - mn.z;
    float diag = std::sqrt(dx * dx + dy * dy + dz * dz);
    _impl->distance   = std::max(diag / (2.f * std::tan(_impl->fov * 0.5f)) * 1.1f, 0.1f);
    _impl->orthoScale = std::max({dx, dy, dz}) * 0.6f;
    _impl->position   = {_impl->target.x, _impl->target.y, _impl->target.z + _impl->distance};
}

// ─── Input processing ─────────────────────────────────────────────────────────

void Camera3D::ProcessOrbitMouseDrag(float dx, float dy) noexcept {
    _impl->yaw   += dx * _impl->orbitSens;
    _impl->pitch -= dy * _impl->orbitSens;
    _impl->pitch  = std::clamp(_impl->pitch, -1.5f, 1.5f);
}

void Camera3D::ProcessOrbitPan(float dx, float dy) noexcept {
    Vec3 eye = _impl->orbitEye();
    Vec3 fwd = norm3(sub3(_impl->target, eye));
    Vec3 r   = norm3(cross3(fwd, {0.f, 1.f, 0.f}));
    Vec3 u   = cross3(r, fwd);
    float s  = _impl->distance * _impl->panSens;
    _impl->target.x += (-r.x * dx + u.x * dy) * s;
    _impl->target.y += (-r.y * dx + u.y * dy) * s;
    _impl->target.z += (-r.z * dx + u.z * dy) * s;
}

void Camera3D::ProcessZoom(float delta) noexcept {
    _impl->distance -= delta * _impl->distance * _impl->zoomSens;
    _impl->distance  = std::max(_impl->distance, 0.01f);
}

void Camera3D::ProcessFlyMouseLook(float dx, float dy) noexcept {
    _impl->yaw   += dx * _impl->lookSens;
    _impl->pitch -= dy * _impl->lookSens;
    _impl->pitch  = std::clamp(_impl->pitch, -1.5f, 1.5f);
}

void Camera3D::ProcessFlyMovement(float forward, float right, float up, float dt) noexcept {
    float speed = _impl->moveSpeed * dt;
    Vec3 fwd    = norm3(_impl->flyForward());
    Vec3 r      = norm3(cross3(fwd, {0.f, 1.f, 0.f}));
    Vec3 u      = cross3(r, fwd);
    _impl->position.x += (fwd.x * forward + r.x * right + u.x * up) * speed;
    _impl->position.y += (fwd.y * forward + r.y * right + u.y * up) * speed;
    _impl->position.z += (fwd.z * forward + r.z * right + u.z * up) * speed;
}

void Camera3D::ProcessOrthoMousePan(float dx, float dy) noexcept {
    Vec3 eye = _impl->orbitEye();
    Vec3 fwd = norm3(sub3(_impl->target, eye));
    Vec3 r   = norm3(cross3(fwd, {0.f, 1.f, 0.f}));
    Vec3 u   = cross3(r, fwd);
    float s  = _impl->orthoScale * _impl->panSens * 2.f;
    _impl->target.x += (-r.x * dx + u.x * dy) * s;
    _impl->target.y += (-r.y * dx + u.y * dy) * s;
    _impl->target.z += (-r.z * dx + u.z * dy) * s;
}

void Camera3D::ProcessOrthoZoom(float delta) noexcept {
    _impl->orthoScale -= delta * _impl->orthoScale * _impl->zoomSens;
    _impl->orthoScale  = std::max(_impl->orthoScale, 0.01f);
}

} // namespace ImFrame::Rendering
