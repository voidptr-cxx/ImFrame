/**
 * @file     Viewport3D.cpp
 * @brief    Viewport3D builder, camera-interaction Show(), Unproject/Project
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

#include "ImFrame/Rendering/Viewport3D.hpp"
#include "ImFrame/Rendering/Viewport.hpp"

#include <imgui.h>

#include <cmath>
#include <cstring>

using Vec2 = ImFrame::Widgets::Vec2;
using Vec3 = ImFrame::Widgets::Vec3;

namespace ImFrame::Rendering {

// ─── Impl ─────────────────────────────────────────────────────────────────────

struct Viewport3D::Impl {
    Rendering::Viewport viewport;
    Camera3D            camera;
    bool                cameraControl   = true;
    CameraMode          interactionMode = CameraMode::Orbit;

    Utility::Delegate<void(const Viewport3DRenderInfo&)> userOnRender;

    Vec2  viewSize = {800.f, 600.f};
    Vec2  viewMin  = {0.f, 0.f};
    float viewMat[16] = {};
    float projMat[16] = {};
    float vpMat[16]   = {};

    explicit Impl(std::string_view id) : viewport(id) {}
};

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Viewport3D::Viewport3D(std::string_view id)
    : _impl(std::make_unique<Impl>(id))
{
    Impl* impl = _impl.get();

    _impl->viewport.OnResize([impl](Vec2 sz) {
        impl->viewSize = sz;
    });

    _impl->viewport.OnRender([impl](const RenderContext& ctx) {
        float aspect = (impl->viewSize.y > 0.f)
                     ? (impl->viewSize.x / impl->viewSize.y) : 1.f;

        Viewport3DRenderInfo info{};
        info.Context = &ctx;
        impl->camera.ViewMatrix(info.ViewMatrix);
        impl->camera.ProjectionMatrix(info.ProjectionMatrix, aspect);
        impl->camera.ViewProjectionMatrix(info.ViewProjectionMatrix, aspect);
        info.CameraPosition  = impl->camera.GetPosition();
        info.CameraDirection = impl->camera.GetDirection();
        info.NearPlane       = impl->camera.GetNearPlane();
        info.FarPlane        = impl->camera.GetFarPlane();
        info.FieldOfView     = (impl->camera.GetMode() == CameraMode::Orthographic)
                             ? 0.f : impl->camera.GetFieldOfView();

        std::memcpy(impl->viewMat, info.ViewMatrix,           sizeof(impl->viewMat));
        std::memcpy(impl->projMat, info.ProjectionMatrix,     sizeof(impl->projMat));
        std::memcpy(impl->vpMat,   info.ViewProjectionMatrix, sizeof(impl->vpMat));

        if (impl->userOnRender) impl->userOnRender(info);
    });
}

Viewport3D::~Viewport3D() noexcept = default;
Viewport3D::Viewport3D(Viewport3D&&) noexcept            = default;
Viewport3D& Viewport3D::operator=(Viewport3D&&) noexcept = default;

// ─── Builder setters ──────────────────────────────────────────────────────────

Viewport3D& Viewport3D::Size(Vec2 size) {
    _impl->viewport.Size(size);
    return *this;
}

Viewport3D& Viewport3D::CameraControl(bool enabled) {
    _impl->cameraControl = enabled;
    return *this;
}

Viewport3D& Viewport3D::InteractionMode(CameraMode mode) {
    _impl->interactionMode = mode;
    _impl->camera.SetMode(mode);
    return *this;
}

Viewport3D& Viewport3D::OnRender(Utility::Delegate<void(const Viewport3DRenderInfo&)> cb) {
    _impl->userOnRender = std::move(cb);
    return *this;
}

// ─── Camera access ────────────────────────────────────────────────────────────

Camera3D&       Viewport3D::GetCamera() noexcept       { return _impl->camera; }
const Camera3D& Viewport3D::GetCamera() const noexcept { return _impl->camera; }

// ─── Projection utilities ─────────────────────────────────────────────────────

Ray3D Viewport3D::Unproject(Vec2 screenPos) const noexcept {
    auto& im = *_impl;
    const float* v = im.viewMat;

    float ndcX = (screenPos.x - im.viewMin.x) / im.viewSize.x * 2.f - 1.f;
    float ndcY = 1.f - (screenPos.y - im.viewMin.y) / im.viewSize.y * 2.f;

    if (im.camera.GetMode() == CameraMode::Orthographic) {
        float aspect  = (im.viewSize.y > 0.f) ? im.viewSize.x / im.viewSize.y : 1.f;
        float halfW   = im.camera.GetOrthoScale() * aspect * 0.5f;
        float halfH   = im.camera.GetOrthoScale() * 0.5f;
        Vec3  eye     = im.camera.GetPosition();
        // V col-major: right=(V[0],V[4],V[8]), up=(V[1],V[5],V[9]), -fwd=(V[2],V[6],V[10])
        Vec3 r = {v[0], v[4], v[8]};
        Vec3 u = {v[1], v[5], v[9]};
        Vec3 f = {-v[2], -v[6], -v[10]};
        Vec3 origin = {
            eye.x + r.x * ndcX * halfW + u.x * ndcY * halfH,
            eye.y + r.y * ndcX * halfW + u.y * ndcY * halfH,
            eye.z + r.z * ndcX * halfW + u.z * ndcY * halfH
        };
        return Ray3D{origin, f};
    }

    float fov        = im.camera.GetFieldOfView();
    float aspect     = (im.viewSize.y > 0.f) ? im.viewSize.x / im.viewSize.y : 1.f;
    float tanHalfFov = std::tan(fov * 0.5f);
    float vdX        = ndcX * aspect * tanHalfFov;
    float vdY        = ndcY * tanHalfFov;
    // World direction = R^T * (vdX, vdY, -1); R rows in V: V[col*4+0..2]
    Vec3 dir = {
        v[0] * vdX + v[1] * vdY - v[2],
        v[4] * vdX + v[5] * vdY - v[6],
        v[8] * vdX + v[9] * vdY - v[10]
    };
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-8f) { dir.x /= len; dir.y /= len; dir.z /= len; }

    return Ray3D{im.camera.GetPosition(), dir};
}

std::optional<Vec2> Viewport3D::Project(Vec3 worldPos) const noexcept {
    const float* vp = _impl->vpMat;
    // clip = VP * [x,y,z,1]^T  (column-major matrix-vector multiply)
    float cx = vp[0]*worldPos.x + vp[4]*worldPos.y + vp[8]*worldPos.z  + vp[12];
    float cy = vp[1]*worldPos.x + vp[5]*worldPos.y + vp[9]*worldPos.z  + vp[13];
    float cw = vp[3]*worldPos.x + vp[7]*worldPos.y + vp[11]*worldPos.z + vp[15];

    if (cw <= 0.f) return std::nullopt;

    float ndcX = cx / cw;
    float ndcY = cy / cw;

    if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f) return std::nullopt;

    auto& im = *_impl;
    return Vec2{
        im.viewMin.x + (ndcX + 1.f) * 0.5f * im.viewSize.x,
        im.viewMin.y + (1.f - ndcY) * 0.5f * im.viewSize.y
    };
}

// ─── Viewport rect / matrix access ───────────────────────────────────────────

Vec2         Viewport3D::GetViewportScreenMin()        const noexcept { return _impl->viewMin; }
Vec2         Viewport3D::GetViewportScreenSize()       const noexcept { return _impl->viewSize; }
const float* Viewport3D::GetViewMatrix()               const noexcept { return _impl->viewMat; }
const float* Viewport3D::GetProjectionMatrix()         const noexcept { return _impl->projMat; }
const float* Viewport3D::GetViewProjectionMatrix()     const noexcept { return _impl->vpMat; }

// ─── Show() ───────────────────────────────────────────────────────────────────

void Viewport3D::Show() {
    auto& im = *_impl;

    // Capture top-left before Show() moves the cursor.
    const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    im.viewMin = {cursorPos.x, cursorPos.y};

    im.viewport.Show();

    // Recache matrices with latest camera state (user may have mutated camera
    // between DispatchViewportRenders and Show).
    float aspect = (im.viewSize.y > 0.f) ? (im.viewSize.x / im.viewSize.y) : 1.f;
    im.camera.ViewMatrix(im.viewMat);
    im.camera.ProjectionMatrix(im.projMat, aspect);
    im.camera.ViewProjectionMatrix(im.vpMat, aspect);

    if (!im.cameraControl || !ImGui::IsItemHovered()) return;

    const ImGuiIO& io = ImGui::GetIO();
    float          dx = io.MouseDelta.x;
    float          dy = io.MouseDelta.y;

    switch (im.interactionMode) {
    case CameraMode::Orbit:
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            im.camera.ProcessOrbitMouseDrag(dx, dy);
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            im.camera.ProcessOrbitPan(dx, dy);
        }
        if (io.MouseWheel != 0.f) {
            im.camera.ProcessZoom(io.MouseWheel);
        }
        break;

    case CameraMode::Fly: {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && (dx != 0.f || dy != 0.f)) {
            im.camera.ProcessFlyMouseLook(dx, dy);
        }
        float dt       = io.DeltaTime;
        float fwd      = 0.f, right = 0.f, up = 0.f;
        if (ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_UpArrow))    fwd   += 1.f;
        if (ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_DownArrow))  fwd   -= 1.f;
        if (ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_RightArrow)) right += 1.f;
        if (ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_LeftArrow))  right -= 1.f;
        if (ImGui::IsKeyDown(ImGuiKey_E)) up += 1.f;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) up -= 1.f;
        if (fwd != 0.f || right != 0.f || up != 0.f) {
            float mult = (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift))
                       ? im.camera.GetSpeedMultiplier() : 1.f;
            im.camera.ProcessFlyMovement(fwd * mult, right * mult, up * mult, dt);
        }
        break;
    }

    case CameraMode::Orthographic:
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
            ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            im.camera.ProcessOrthoMousePan(dx, dy);
        }
        if (io.MouseWheel != 0.f) {
            im.camera.ProcessOrthoZoom(io.MouseWheel);
        }
        break;
    }
}

} // namespace ImFrame::Rendering

// ─── Viewport3DWidget / Viewport3DElement (Phase 29) ────────────────────────────

namespace ImFrame::Internal {

class Viewport3DElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Rendering::Viewport3DWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Rendering::Viewport3DWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        _size = {constraints.MaxWidth, constraints.MaxHeight};
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 position) override {
        Rendering::Viewport3D* viewport = _config.GetViewport();
        if (!viewport) { return; }
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        viewport->Show();
    }

private:
    Rendering::Viewport3DWidget _config{nullptr};
};

} // namespace ImFrame::Internal

namespace ImFrame::Rendering {

std::unique_ptr<Tree::Element> Viewport3DWidget::CreateElement() const {
    return std::make_unique<Internal::Viewport3DElement>();
}

} // namespace ImFrame::Rendering
