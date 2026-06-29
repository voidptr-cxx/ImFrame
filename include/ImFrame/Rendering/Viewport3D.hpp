/**
 * @file     Viewport3D.hpp
 * @brief    3D viewport widget with Camera3D, Unproject/Project, and gizmo surface
 *
 * `Viewport3D` wraps a `Viewport` and exposes a higher-level interface for 3D
 * rendering. The user's `OnRender` callback receives a `Viewport3DRenderInfo`
 * that carries all camera matrices and backend framebuffer handles for the
 * current frame. After `Show()`, the matrices are cached so that `Unproject()`
 * and `Project()` can be called from `OnUi()` in the same frame.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-29
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Rendering/Camera3D.hpp"
#include "ImFrame/Rendering/Ray3D.hpp"
#include "ImFrame/Rendering/RenderContext.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <memory>
#include <optional>
#include <string_view>

namespace ImFrame::Rendering {

// ─── Viewport3DRenderInfo ─────────────────────────────────────────────────────

/**
 * @struct Viewport3DRenderInfo
 * @brief  Per-frame render parameters delivered to the Viewport3D OnRender callback
 *
 * `Context` is non-null for the duration of the callback only — do not retain
 * the pointer. All matrices are column-major (OpenGL/Vulkan/Metal convention).
 *
 * @since  2.1.0
 */
struct Viewport3DRenderInfo {
    const RenderContext* Context;         ///< Backend render context (framebuffer + GPU handles). Valid during callback only.
    float ViewMatrix[16];                 ///< Column-major world-to-camera matrix.
    float ProjectionMatrix[16];           ///< Column-major projection matrix.
    float ViewProjectionMatrix[16];       ///< Column-major P * V product.
    Widgets::Vec3 CameraPosition;         ///< World-space eye position.
    Widgets::Vec3 CameraDirection;        ///< Normalized camera forward direction.
    float NearPlane;                      ///< Near clip plane distance.
    float FarPlane;                       ///< Far clip plane distance.
    float FieldOfView;                    ///< Vertical FOV in radians (0 for Orthographic mode).
};

// ─── Viewport3D ───────────────────────────────────────────────────────────────

/**
 * @class    Viewport3D
 * @brief    3D scene viewport widget with Camera3D, Unproject/Project, and gizmo surface
 *
 * Follows the Phase 10–14 builder pattern: construct, configure via fluent
 * setters, call `Show()` once per frame from `OnUi`.
 *
 * `Unproject()` and `Project()` are valid after `Show()` has been called in
 * the same frame. Calling them before `Show()` returns results from the
 * previous frame.
 *
 * @note  Non-copyable. Move is allowed before first `Show()`.
 * @since 2.1.0
 *
 * @example
 * @code
 * Rendering::Viewport3D vp("scene");
 * vp.Size({800, 600})
 *   .InteractionMode(CameraMode::Orbit)
 *   .OnRender([](const Rendering::Viewport3DRenderInfo& info) {
 *       auto& gl = std::get<Rendering::ViewportImageGL>(info.Context->NativeImage);
 *       glBindFramebuffer(GL_FRAMEBUFFER, gl.Framebuffer);
 *       // upload info.ViewProjectionMatrix to shader, render scene
 *       glBindFramebuffer(GL_FRAMEBUFFER, 0);
 *   });
 * // In OnUi:
 * vp.Show();
 * // After Show():
 * Ray3D ray = vp.Unproject(mousePos);
 * @endcode
 */
class Viewport3D {
public:
    /**
     * @brief    Construct a Viewport3D with a stable per-widget ID.
     * @param[in] id  Unique string identifier stable across frames.
     */
    explicit Viewport3D(std::string_view id);

    ~Viewport3D() noexcept;

    Viewport3D(const Viewport3D&)            = delete;
    Viewport3D& operator=(const Viewport3D&) = delete;
    Viewport3D(Viewport3D&&)                 noexcept;
    Viewport3D& operator=(Viewport3D&&)      noexcept;

    // ─── Builder setters ──────────────────────────────────────────────────────

    /**
     * @brief    Pin the viewport to a fixed pixel size.
     *
     * When not called, the viewport expands to fill the parent window.
     *
     * @param[in] size  Pixel dimensions; both components must be > 0.
     * @return  Reference to this Viewport3D for chaining.
     */
    Viewport3D& Size(Widgets::Vec2 size);

    /**
     * @brief    Enable or disable automatic camera mouse/keyboard interaction.
     *
     * Default: `true`. When `false`, the camera state can still be updated
     * manually via `GetCamera()`.
     *
     * @param[in] enabled  `true` to enable built-in interaction.
     * @return  Reference to this Viewport3D for chaining.
     */
    Viewport3D& CameraControl(bool enabled);

    /**
     * @brief    Set the camera interaction mode.
     * @param[in] mode  Orbit, Fly, or Orthographic.
     * @return  Reference to this Viewport3D for chaining.
     */
    Viewport3D& InteractionMode(CameraMode mode);

    /**
     * @brief    Register the render callback invoked before each ImGui frame.
     *
     * The callback fires from `DispatchViewportRenders()` before `BeginFrame()`.
     * All GPU work for this viewport must be submitted or recorded before the
     * callback returns.
     *
     * @param[in] callback  Callable accepting a `const Viewport3DRenderInfo&`.
     * @return  Reference to this Viewport3D for chaining.
     */
    Viewport3D& OnRender(Utility::Delegate<void(const Viewport3DRenderInfo&)> callback);

    // ─── Camera access ────────────────────────────────────────────────────────

    /**
     * @brief  Direct mutable access to the owned Camera3D.
     * @return  Reference to the internal Camera3D.
     */
    [[nodiscard]] Camera3D&       GetCamera() noexcept;

    /**
     * @brief  Read-only access to the owned Camera3D.
     * @return  Const reference to the internal Camera3D.
     */
    [[nodiscard]] const Camera3D& GetCamera() const noexcept;

    // ─── Projection utilities (valid after Show()) ────────────────────────────

    /**
     * @brief    Convert a screen-space position to a world-space ray.
     *
     * Uses the matrices cached by the most recent `Show()` call. Calling
     * before `Show()` in the current frame returns a stale result.
     *
     * @param[in] screenPos  Position in screen pixels (absolute, not relative to viewport).
     * @return   World-space ray with normalized direction.
     */
    [[nodiscard]] Ray3D Unproject(Widgets::Vec2 screenPos) const noexcept;

    /**
     * @brief    Project a world-space position to screen-space coordinates.
     *
     * Uses the matrices cached by the most recent `Show()` call.
     *
     * @param[in] worldPos  World-space point to project.
     * @return   Screen-space pixel position, or `nullopt` if the point is
     *           behind the camera or outside the viewport bounds.
     */
    [[nodiscard]] std::optional<Widgets::Vec2> Project(Widgets::Vec3 worldPos) const noexcept;

    // ─── Viewport rect (for Gizmo) ────────────────────────────────────────────

    [[nodiscard]] Widgets::Vec2 GetViewportScreenMin()  const noexcept; ///< Top-left screen position after Show().
    [[nodiscard]] Widgets::Vec2 GetViewportScreenSize() const noexcept; ///< Pixel size after Show().

    // ─── Cached matrix access (for Gizmo) ────────────────────────────────────

    [[nodiscard]] const float* GetViewMatrix()           const noexcept; ///< 16-element column-major view matrix.
    [[nodiscard]] const float* GetProjectionMatrix()     const noexcept; ///< 16-element column-major projection matrix.
    [[nodiscard]] const float* GetViewProjectionMatrix() const noexcept; ///< 16-element column-major VP matrix.

    // ─── Terminal ─────────────────────────────────────────────────────────────

    /**
     * @brief    Composite this Viewport3D into the current ImGui window for one frame.
     *
     * Renders the scene image, drives camera interaction (if `CameraControl`
     * is enabled), and caches the viewport screen rect and matrices for use
     * by `Unproject()`, `Project()`, and `Gizmo::Show()`.
     */
    void Show();

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace ImFrame::Rendering
