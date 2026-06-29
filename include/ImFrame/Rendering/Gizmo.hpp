/**
 * @file     Gizmo.hpp
 * @brief    3D manipulation handles for translate, rotate, and scale operations
 *
 * `Gizmo` draws colored axis handles on top of a `Viewport3D` scene using
 * `ImGui::GetWindowDrawList()`. No user-renderer changes are required.
 *
 * Call `Gizmo::Show()` after `Viewport3D::Show()` in the same `OnUi` frame.
 * `Show()` returns `true` while the user is dragging a handle, allowing the
 * caller to respond to transform changes.
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

#include "ImFrame/Rendering/Transform3D.hpp"
#include "ImFrame/Rendering/Viewport3D.hpp"

namespace ImFrame::Rendering {

// ─── GizmoMode ────────────────────────────────────────────────────────────────

/**
 * @enum  GizmoMode
 * @brief Selects which manipulation handles the Gizmo renders
 * @since 2.1.0
 */
enum class GizmoMode : unsigned char {
    Translate, ///< Three colored axis arrows; dragging moves the transform.
    Rotate,    ///< Three colored arc rings; dragging rotates the transform.
    Scale,     ///< Three colored axis handles; dragging scales the transform.
    Universal, ///< Combined translate + rotate handles.
};

// ─── GizmoSpace ───────────────────────────────────────────────────────────────

/**
 * @enum  GizmoSpace
 * @brief Coordinate space for the gizmo handles
 * @since 2.1.0
 */
enum class GizmoSpace : unsigned char {
    World, ///< Handles aligned to world axes.
    Local, ///< Handles aligned to the transform's local axes.
};

// ─── Gizmo ────────────────────────────────────────────────────────────────────

/**
 * @class    Gizmo
 * @brief    Per-frame 3D manipulation widget rendered as an ImDrawList overlay
 *
 * Construct once per object you wish to manipulate; call `Show()` each frame
 * after the owning `Viewport3D::Show()`.
 *
 * @since    2.1.0
 *
 * @example
 * @code
 * Transform3D t = Transform3D::Identity();
 * Gizmo gizmo(viewport3d);
 *
 * // In OnUi:
 * viewport3d.Show();
 * bool changed = gizmo.Transform(t).Mode(GizmoMode::Translate).Show();
 * @endcode
 */
class Gizmo {
public:
    /**
     * @brief    Construct a Gizmo bound to a Viewport3D.
     * @param[in] viewport  The viewport over which this gizmo will draw.
     * @throws   Nothing — noexcept.
     */
    explicit Gizmo(Viewport3D& viewport) noexcept;

    /**
     * @brief    Bind this gizmo to a transform for the current `Show()` call.
     * @param[in] transform  Transform to read from and write to on drag.
     * @return   Reference to this Gizmo for chaining.
     */
    Gizmo& Transform(Rendering::Transform3D& transform) noexcept;

    /**
     * @brief    Set the manipulation mode.
     * @param[in] mode  Translate, Rotate, Scale, or Universal.
     * @return   Reference to this Gizmo for chaining.
     */
    Gizmo& Mode(GizmoMode mode) noexcept;

    /**
     * @brief    Set the coordinate space for the handles.
     * @param[in] space  World or Local.
     * @return   Reference to this Gizmo for chaining.
     */
    Gizmo& Space(GizmoSpace space) noexcept;

    /**
     * @brief    Render the gizmo handles and process interaction.
     *
     * Must be called after `Viewport3D::Show()` in the same `OnUi` frame.
     * Draws on `ImGui::GetWindowDrawList()` at the viewport's screen rect.
     *
     * @return   `true` while the user is actively dragging a handle;
     *           `false` when idle (no transform has been modified).
     */
    bool Show();

private:
    Viewport3D*            _viewport  = nullptr;
    Rendering::Transform3D* _transform = nullptr;
    GizmoMode              _mode      = GizmoMode::Translate;
    GizmoSpace             _space     = GizmoSpace::World;

    int  _activeAxis = -1; ///< 0 = X, 1 = Y, 2 = Z, -1 = none.
    bool _dragging   = false;
};

} // namespace ImFrame::Rendering
