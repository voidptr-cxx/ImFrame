/**
 * @file     Gizmo.cpp
 * @brief    Gizmo axis rendering and drag interaction on Viewport3D
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-29
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Rendering/Gizmo.hpp"

#include <imgui.h>

#include <cmath>

using Vec2 = ImFrame::Widgets::Vec2;
using Vec3 = ImFrame::Widgets::Vec3;

namespace {

// ─── Internal math helpers ────────────────────────────────────────────────────

float dot2(Vec2 a, Vec2 b) noexcept { return a.x * b.x + a.y * b.y; }

Vec2 sub2(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }

float len2(Vec2 v) noexcept { return std::sqrt(v.x * v.x + v.y * v.y); }

// Project a world-space position through VP into screen space.
// Returns false if the point is behind the near plane.
bool worldToScreen(Vec3 worldPos, const float vp[16], Vec2 viewMin, Vec2 viewSize, Vec2& out) noexcept {
    float cx = vp[0]*worldPos.x + vp[4]*worldPos.y + vp[8]*worldPos.z  + vp[12];
    float cy = vp[1]*worldPos.x + vp[5]*worldPos.y + vp[9]*worldPos.z  + vp[13];
    float cw = vp[3]*worldPos.x + vp[7]*worldPos.y + vp[11]*worldPos.z + vp[15];
    if (cw <= 0.f) return false;
    float ndcX = cx / cw;
    float ndcY = cy / cw;
    out.x = viewMin.x + (ndcX + 1.f) * 0.5f * viewSize.x;
    out.y = viewMin.y + (1.f - ndcY) * 0.5f * viewSize.y;
    return true;
}

// Axis world direction vectors.
constexpr Vec3 AXIS_DIRS[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
constexpr ImU32 AXIS_COLORS[3] = {
    IM_COL32(220,  50,  50, 255), // X — red
    IM_COL32( 50, 200,  50, 255), // Y — green
    IM_COL32( 50,  50, 220, 255), // Z — blue
};
constexpr ImU32 AXIS_HOVER_COLOR   = IM_COL32(255, 220,   0, 255);
constexpr ImU32 AXIS_DRAGGING_COLOR = IM_COL32(255, 255, 100, 255);

constexpr float ARROW_LEN_NDC  = 0.12f; // handle length as fraction of min viewport dim
constexpr float HIT_RADIUS_PX  = 8.f;
constexpr float ARROW_THICKNESS = 2.5f;
constexpr float ARROWHEAD_SIZE  = 10.f;

} // anonymous namespace

namespace ImFrame::Rendering {

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Gizmo::Gizmo(Viewport3D& viewport) noexcept
    : _viewport(&viewport) {}

// ─── Builder setters ──────────────────────────────────────────────────────────

Gizmo& Gizmo::Transform(Rendering::Transform3D& t) noexcept  { _transform = &t; return *this; }
Gizmo& Gizmo::Mode(GizmoMode m) noexcept                     { _mode = m; return *this; }
Gizmo& Gizmo::Space(GizmoSpace s) noexcept                   { _space = s; return *this; }

// ─── Show() ───────────────────────────────────────────────────────────────────

bool Gizmo::Show() {
    if (!_viewport || !_transform) return false;

    Vec2  viewMin  = _viewport->GetViewportScreenMin();
    Vec2  viewSize = _viewport->GetViewportScreenSize();
    float minDim   = (viewSize.x < viewSize.y) ? viewSize.x : viewSize.y;

    const float*  vp       = _viewport->GetViewProjectionMatrix();
    Vec3          origin   = _transform->Position;

    // Compute screen positions of axis endpoints.
    float arrowLen = minDim * ARROW_LEN_NDC;
    Vec2  originSS{};
    if (!worldToScreen(origin, vp, viewMin, viewSize, originSS)) {
        _dragging   = false;
        _activeAxis = -1;
        return false;
    }

    // Compute tip screen positions; axis directions may be local or world.
    Vec2 tipSS[3]{};
    bool tipValid[3]{};
    for (int i = 0; i < 3; ++i) {
        Vec3 dir = AXIS_DIRS[i];
        if (_space == GizmoSpace::Local) {
            // Rotate dir by the transform's quaternion.
            const auto& q = _transform->Rotation;
            float qx = q[0], qy = q[1], qz = q[2], qw = q[3];
            float t2x = 2.f * (qy*dir.z - qz*dir.y);
            float t2y = 2.f * (qz*dir.x - qx*dir.z);
            float t2z = 2.f * (qx*dir.y - qy*dir.x);
            dir = {
                dir.x + qw*t2x + qy*t2z - qz*t2y,
                dir.y + qw*t2y + qz*t2x - qx*t2z,
                dir.z + qw*t2z + qx*t2y - qy*t2x
            };
        }
        // Place tip 1 world-unit away from origin, then extend in screen space
        // to a fixed pixel length so the gizmo has consistent screen size.
        Vec3 tip1u = {origin.x + dir.x, origin.y + dir.y, origin.z + dir.z};
        Vec2 tip1uSS{};
        if (worldToScreen(tip1u, vp, viewMin, viewSize, tip1uSS)) {
            Vec2 screenDir = sub2(tip1uSS, originSS);
            float slen     = len2(screenDir);
            if (slen > 0.5f) {
                float scale = arrowLen / slen;
                tipSS[i]    = {originSS.x + screenDir.x * scale,
                               originSS.y + screenDir.y * scale};
                tipValid[i] = true;
            }
        }
    }

    // ── Hit test ──────────────────────────────────────────────────────────────
    ImVec2 mousePos = ImGui::GetMousePos();
    Vec2   mouse    = {mousePos.x, mousePos.y};

    bool mouseInViewport = (mouse.x >= viewMin.x && mouse.x <= viewMin.x + viewSize.x &&
                            mouse.y >= viewMin.y && mouse.y <= viewMin.y + viewSize.y);

    if (!_dragging && mouseInViewport && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        _activeAxis = -1;
        float bestDist = HIT_RADIUS_PX;
        for (int i = 0; i < 3; ++i) {
            if (!tipValid[i]) continue;
            // Distance from mouse to line segment (origin → tip).
            Vec2  seg  = sub2(tipSS[i], originSS);
            Vec2  toM  = sub2(mouse, originSS);
            float segL = len2(seg);
            float d;
            if (segL < 1.f) {
                d = len2(toM);
            } else {
                float t = dot2(toM, seg) / (segL * segL);
                t = (t < 0.f) ? 0.f : (t > 1.f) ? 1.f : t;
                Vec2 proj = {originSS.x + seg.x * t, originSS.y + seg.y * t};
                d = len2(sub2(mouse, proj));
            }
            if (d < bestDist) { bestDist = d; _activeAxis = i; }
        }
        if (_activeAxis >= 0) _dragging = true;
    }

    if (_dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        _dragging   = false;
        _activeAxis = -1;
    }

    // ── Apply drag ────────────────────────────────────────────────────────────
    if (_dragging && _activeAxis >= 0) {
        Vec2 delta = {ImGui::GetIO().MouseDelta.x, ImGui::GetIO().MouseDelta.y};

        if (_mode == GizmoMode::Translate || _mode == GizmoMode::Universal) {
            // Project mouse delta onto screen-space axis direction.
            if (tipValid[_activeAxis]) {
                Vec2 axisSS = sub2(tipSS[_activeAxis], originSS);
                float axLen = len2(axisSS);
                if (axLen > 0.5f) {
                    Vec2 axNorm  = {axisSS.x / axLen, axisSS.y / axLen};
                    float amount = dot2(delta, axNorm);
                    // Scale: arrowLen pixels ≈ 1 world unit along this axis.
                    float worldAmount = amount / arrowLen;
                    Vec3  dir = (_space == GizmoSpace::Local)
                              ? Vec3{} // computed below
                              : AXIS_DIRS[_activeAxis];
                    if (_space == GizmoSpace::Local) {
                        const auto& q = _transform->Rotation;
                        float qx=q[0],qy=q[1],qz=q[2],qw=q[3];
                        Vec3 d0 = AXIS_DIRS[_activeAxis];
                        float t2x=2.f*(qy*d0.z-qz*d0.y);
                        float t2y=2.f*(qz*d0.x-qx*d0.z);
                        float t2z=2.f*(qx*d0.y-qy*d0.x);
                        dir={d0.x+qw*t2x+qy*t2z-qz*t2y,
                             d0.y+qw*t2y+qz*t2x-qx*t2z,
                             d0.z+qw*t2z+qx*t2y-qy*t2x};
                    }
                    _transform->Position.x += dir.x * worldAmount;
                    _transform->Position.y += dir.y * worldAmount;
                    _transform->Position.z += dir.z * worldAmount;
                }
            }

        } else if (_mode == GizmoMode::Scale) {
            if (tipValid[_activeAxis]) {
                Vec2 axisSS = sub2(tipSS[_activeAxis], originSS);
                float axLen = len2(axisSS);
                if (axLen > 0.5f) {
                    Vec2  axNorm = {axisSS.x / axLen, axisSS.y / axLen};
                    float amount = dot2(delta, axNorm) / arrowLen;
                    float* sc    = &_transform->Scale.x;
                    sc[_activeAxis] += amount;
                    if (sc[_activeAxis] < 0.0001f) sc[_activeAxis] = 0.0001f;
                }
            }

        } else if (_mode == GizmoMode::Rotate) {
            // Approximate rotation: drag perpendicular to axis → angle change.
            if (tipValid[_activeAxis]) {
                Vec2 axisSS = sub2(tipSS[_activeAxis], originSS);
                float axLen = len2(axisSS);
                if (axLen > 0.5f) {
                    // Perpendicular in screen space.
                    Vec2  perp    = {-axisSS.y / axLen, axisSS.x / axLen};
                    float angle   = dot2(delta, perp) / arrowLen * 1.5f;
                    // Apply rotation: compose quaternion q_delta * q_current.
                    Vec3  axis    = (_space == GizmoSpace::Local)
                                  ? AXIS_DIRS[_activeAxis] : AXIS_DIRS[_activeAxis];
                    float ha      = angle * 0.5f;
                    float sinHa   = std::sin(ha), cosHa = std::cos(ha);
                    float dqx     = axis.x * sinHa, dqy = axis.y * sinHa, dqz = axis.z * sinHa, dqw = cosHa;
                    auto& q       = _transform->Rotation;
                    float ox=q[0],oy=q[1],oz=q[2],ow=q[3];
                    q[0] = dqw*ox + dqx*ow + dqy*oz - dqz*oy;
                    q[1] = dqw*oy - dqx*oz + dqy*ow + dqz*ox;
                    q[2] = dqw*oz + dqx*oy - dqy*ox + dqz*ow;
                    q[3] = dqw*ow - dqx*ox - dqy*oy - dqz*oz;
                    // Renormalize.
                    float qlen = std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
                    if (qlen > 1e-8f) { q[0]/=qlen; q[1]/=qlen; q[2]/=qlen; q[3]/=qlen; }
                }
            }
        }
    }

    // ── Draw ──────────────────────────────────────────────────────────────────
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < 3; ++i) {
        if (!tipValid[i]) continue;

        ImU32 color;
        if (_dragging && _activeAxis == i) {
            color = AXIS_DRAGGING_COLOR;
        } else if (!_dragging && mouseInViewport && _activeAxis == i) {
            color = AXIS_HOVER_COLOR;
        } else {
            color = AXIS_COLORS[i];
        }

        ImVec2 o  = {originSS.x, originSS.y};
        ImVec2 t  = {tipSS[i].x, tipSS[i].y};

        // Shaft.
        dl->AddLine(o, t, color, ARROW_THICKNESS);

        // Arrowhead triangle.
        Vec2 shaft = sub2({t.x, t.y}, {o.x, o.y});
        float sLen = len2(shaft);
        if (sLen > 0.5f) {
            Vec2  dn  = {shaft.x / sLen, shaft.y / sLen};
            Vec2  perp = {-dn.y, dn.x};
            float hs  = ARROWHEAD_SIZE * 0.4f;
            ImVec2 base = {t.x - dn.x * ARROWHEAD_SIZE, t.y - dn.y * ARROWHEAD_SIZE};
            ImVec2 l   = {base.x + perp.x * hs, base.y + perp.y * hs};
            ImVec2 r   = {base.x - perp.x * hs, base.y - perp.y * hs};
            dl->AddTriangleFilled(t, l, r, color);
        }
    }

    // Origin dot.
    dl->AddCircleFilled({originSS.x, originSS.y}, 4.f, IM_COL32(240, 240, 240, 255));

    return _dragging;
}

} // namespace ImFrame::Rendering
