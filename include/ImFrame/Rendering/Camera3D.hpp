/**
 * @file     Camera3D.hpp
 * @brief    3D camera with orbit, fly, and orthographic interaction modes
 *
 * `Camera3D` is owned by `Viewport3D` and drives its view and projection
 * matrices. Three interaction modes are supported (configured via
 * `Viewport3D::InteractionMode()`):
 *   - **Orbit** — rotate/pan/zoom around a focal target.
 *   - **Fly**   — first-person flight with WASD + mouse look.
 *   - **Orthographic** — pan and scale an orthographic projection.
 *
 * All matrix output is column-major to match OpenGL, Vulkan, and Metal
 * column-vector conventions. Users with row-major renderers must transpose.
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

#include "ImFrame/Widgets/Types.hpp"

#include <memory>

namespace ImFrame::Rendering {

// ─── CameraMode ───────────────────────────────────────────────────────────────

/**
 * @enum  CameraMode
 * @brief Controls how `Camera3D` responds to mouse and keyboard input
 * @since 2.1.0
 */
enum class CameraMode : unsigned char {
    Orbit,        ///< Rotate/pan/zoom around a focal target point.
    Fly,          ///< First-person flight: mouse look + WASD movement.
    Orthographic, ///< Orthographic projection; pan and scroll to scale.
};

// ─── Camera3D ─────────────────────────────────────────────────────────────────

/**
 * @class    Camera3D
 * @brief    3D perspective or orthographic camera owned by Viewport3D
 *
 * Access via `Viewport3D::GetCamera()`. All fluent setters return `*this`
 * for chaining.
 *
 * **Orbit** — pitch and yaw place the eye `distance` units from `target`.
 * Left-drag rotates; right-drag pans; scroll zooms.
 *
 * **Fly** — position is tracked directly; pitch and yaw orient the camera.
 * Left-drag looks; WASD/arrows move; Shift multiplies speed.
 *
 * **Orthographic** — orbit navigation with an orthographic projection scaled
 * by `OrthoScale`.
 *
 * @since    2.1.0
 *
 * @example
 * @code
 * auto& cam = viewport3d.GetCamera();
 * cam.SetTarget({0,0,0}).SetDistance(10.f).FieldOfView(1.047f);
 * float view[16];
 * cam.ViewMatrix(view);
 * @endcode
 */
class Camera3D {
public:
    Camera3D();
    ~Camera3D() noexcept;

    Camera3D(const Camera3D&);
    Camera3D& operator=(const Camera3D&);
    Camera3D(Camera3D&&) noexcept;
    Camera3D& operator=(Camera3D&&) noexcept;

    // ─── Matrix output ────────────────────────────────────────────────────────

    /**
     * @brief    Fill a column-major view (world-to-camera) matrix.
     * @param[out] out  16-element float array (column-major, OpenGL convention).
     */
    void ViewMatrix(float out[16]) const noexcept;

    /**
     * @brief    Fill a column-major projection matrix.
     * @param[out] out         16-element float array (column-major).
     * @param[in]  aspectRatio Viewport width / height. Must be > 0.
     */
    void ProjectionMatrix(float out[16], float aspectRatio) const noexcept;

    /**
     * @brief    Fill the column-major view-projection product P * V.
     * @param[out] out         16-element float array (column-major).
     * @param[in]  aspectRatio Viewport width / height. Must be > 0.
     */
    void ViewProjectionMatrix(float out[16], float aspectRatio) const noexcept;

    // ─── Configuration ────────────────────────────────────────────────────────

    /**
     * @brief    Switch the active interaction mode.
     *
     * Called automatically by `Viewport3D::InteractionMode()`.
     *
     * @param[in] mode  New camera mode.
     * @return   Reference to this Camera3D for chaining.
     */
    Camera3D& SetMode(CameraMode mode) noexcept;

    /**
     * @brief  Set the orbit focal target (Orbit and Orthographic modes).
     * @param[in] target  World-space focal point.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& SetTarget(Widgets::Vec3 target) noexcept;

    /**
     * @brief  Set the orbit distance from the target.
     * @param[in] distance  Camera-to-target distance. Clamped to [0.01, ∞).
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& SetDistance(float distance) noexcept;

    /**
     * @brief  Set the camera pitch (shared by Orbit and Fly modes).
     * @param[in] pitch  Angle in radians above the horizon. Clamped to [−1.5, 1.5].
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& SetPitch(float pitch) noexcept;

    /**
     * @brief  Set the camera yaw (shared by Orbit and Fly modes).
     * @param[in] yaw  Horizontal angle in radians.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& SetYaw(float yaw) noexcept;

    /**
     * @brief  Set the explicit camera position (Fly mode).
     * @param[in] position  World-space eye position.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& SetPosition(Widgets::Vec3 position) noexcept;

    /**
     * @brief  Set the orthographic view half-height (Orthographic mode).
     * @param[in] scale  Half-height of the visible volume. Must be > 0.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& OrthoScale(float scale) noexcept;

    /**
     * @brief  Set the near-clip plane distance.
     * @param[in] near  Near plane in world units. Must be > 0.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& NearPlane(float near) noexcept;

    /**
     * @brief  Set the far-clip plane distance.
     * @param[in] far  Far plane in world units. Must be > NearPlane.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& FarPlane(float far) noexcept;

    /**
     * @brief  Set the vertical field of view (perspective only).
     * @param[in] fov  Vertical FOV in radians. Typical: 0.79 (45°) – 1.22 (70°).
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& FieldOfView(float fov) noexcept;

    // ─── Orbit sensitivity ────────────────────────────────────────────────────

    /**
     * @brief  Mouse-pixel-to-radian scale for orbit rotation. Default: 0.01.
     * @param[in] s Sensitivity.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& OrbitSensitivity(float s) noexcept;

    /**
     * @brief  Pan speed relative to camera distance. Default: 0.005.
     * @param[in] s Sensitivity.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& PanSensitivity(float s) noexcept;

    /**
     * @brief  Scroll-to-zoom factor. Default: 0.5.
     * @param[in] s Sensitivity.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& ZoomSensitivity(float s) noexcept;

    // ─── Fly sensitivity ──────────────────────────────────────────────────────

    /**
     * @brief  World-units-per-second fly speed. Default: 5.
     * @param[in] s Speed.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& MovementSpeed(float s) noexcept;

    /**
     * @brief  Mouse-pixel-to-radian scale for fly look. Default: 0.003.
     * @param[in] s Sensitivity.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& LookSensitivity(float s) noexcept;

    /**
     * @brief  Speed multiplier applied while Shift is held. Default: 3.
     * @param[in] m Multiplier.
     * @return  Reference to this Camera3D for chaining.
     */
    Camera3D& SpeedMultiplier(float m) noexcept;

    // ─── Getters ──────────────────────────────────────────────────────────────

    [[nodiscard]] Widgets::Vec3 GetPosition()    const noexcept; ///< Current world-space eye position.
    [[nodiscard]] Widgets::Vec3 GetDirection()   const noexcept; ///< Normalized forward direction.
    [[nodiscard]] Widgets::Vec3 GetTarget()      const noexcept; ///< Orbit focal target.
    [[nodiscard]] float         GetNearPlane()   const noexcept;
    [[nodiscard]] float         GetFarPlane()    const noexcept;
    [[nodiscard]] float         GetFieldOfView() const noexcept; ///< Vertical FOV in radians.
    [[nodiscard]] float         GetOrthoScale()      const noexcept;
    [[nodiscard]] float         GetDistance()        const noexcept;
    [[nodiscard]] CameraMode    GetMode()            const noexcept;
    [[nodiscard]] float         GetSpeedMultiplier() const noexcept;

    // ─── Utility ──────────────────────────────────────────────────────────────

    /**
     * @brief  Reset to defaults: target={0,0,0}, distance=5, pitch=0.3, yaw=0.
     */
    void Reset() noexcept;

    /**
     * @brief    Adjust camera to frame a world-space AABB with a 10% margin.
     *
     * Sets `target` to the AABB centre; adjusts `distance` / `OrthoScale` so
     * the bounding box is fully visible given the current FOV.
     *
     * @param[in] min  Minimum corner of the bounding box.
     * @param[in] max  Maximum corner of the bounding box.
     */
    void FrameExtents(Widgets::Vec3 min, Widgets::Vec3 max) noexcept;

    // ─── Input processing (called by Viewport3D) ──────────────────────────────

    /** @brief Rotate the orbit camera by a mouse-drag delta in pixels. */
    void ProcessOrbitMouseDrag(float dx, float dy) noexcept;
    /** @brief Pan orbit camera (moves target + eye in the view plane). */
    void ProcessOrbitPan(float dx, float dy) noexcept;
    /** @brief Zoom orbit camera. Positive delta = zoom in. */
    void ProcessZoom(float delta) noexcept;
    /** @brief Rotate fly camera by a mouse look-drag delta in pixels. */
    void ProcessFlyMouseLook(float dx, float dy) noexcept;
    /**
     * @brief  Move the fly camera along its local axes.
     * @param[in] forward  Forward/backward component (positive = forward).
     * @param[in] right    Strafe component (positive = right).
     * @param[in] up       Vertical component (positive = up).
     * @param[in] dt       Delta time in seconds.
     */
    void ProcessFlyMovement(float forward, float right, float up, float dt) noexcept;
    /** @brief Pan orthographic camera by a mouse-drag delta in pixels. */
    void ProcessOrthoMousePan(float dx, float dy) noexcept;
    /** @brief Scale orthographic view. Positive delta = zoom in (scale down). */
    void ProcessOrthoZoom(float delta) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace ImFrame::Rendering
