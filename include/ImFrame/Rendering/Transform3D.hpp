/**
 * @file     Transform3D.hpp
 * @brief    3D transform: position, orientation quaternion, and scale
 *
 * `Transform3D` is the data handle passed to `Gizmo::Transform()`. The
 * `Matrix()` helper produces a column-major TRS matrix (translation applied
 * last) compatible with OpenGL, Vulkan, and Metal column-vector conventions.
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

#include <array>

namespace ImFrame::Rendering {

/**
 * @struct Transform3D
 * @brief  Position, rotation quaternion, and non-uniform scale for a 3D object
 *
 * Rotation is stored as a unit quaternion in [x, y, z, w] order (same as
 * GLM, DirectXMath, and most physics engines). The identity quaternion is
 * `{0, 0, 0, 1}`.
 *
 * @since  2.1.0
 *
 * @example
 * @code
 * Transform3D t = Transform3D::Identity();
 * t.Position = {1.f, 2.f, 3.f};
 * float m[16];
 * t.Matrix(m); // column-major TRS matrix
 * @endcode
 */
struct Transform3D {
    Widgets::Vec3        Position = {0.f, 0.f, 0.f}; ///< World-space position.
    std::array<float, 4> Rotation = {0.f, 0.f, 0.f, 1.f}; ///< Unit quaternion [x, y, z, w].
    Widgets::Vec3        Scale    = {1.f, 1.f, 1.f}; ///< Non-uniform scale per axis.

    /**
     * @brief  Return an identity transform (origin, no rotation, unit scale).
     * @return Identity Transform3D.
     */
    [[nodiscard]] static Transform3D Identity() noexcept { return {}; }

    /**
     * @brief    Compute a column-major 4x4 TRS matrix from this transform.
     *
     * The matrix encodes: Scale → Rotate → Translate (right-to-left application
     * order, matching OpenGL/Vulkan/Metal column-vector convention).
     *
     * @param[out] out  16-element float array, column-major (OpenGL convention).
     */
    void Matrix(float out[16]) const noexcept {
        const float qx = Rotation[0], qy = Rotation[1], qz = Rotation[2], qw = Rotation[3];
        const float sx = Scale.x, sy = Scale.y, sz = Scale.z;

        // Column 0 (basis X, scaled)
        out[0] = (1.f - 2.f * (qy * qy + qz * qz)) * sx;
        out[1] = (2.f * (qx * qy + qw * qz))        * sx;
        out[2] = (2.f * (qx * qz - qw * qy))        * sx;
        out[3] = 0.f;

        // Column 1 (basis Y, scaled)
        out[4] = (2.f * (qx * qy - qw * qz))        * sy;
        out[5] = (1.f - 2.f * (qx * qx + qz * qz)) * sy;
        out[6] = (2.f * (qy * qz + qw * qx))        * sy;
        out[7] = 0.f;

        // Column 2 (basis Z, scaled)
        out[8]  = (2.f * (qx * qz + qw * qy))        * sz;
        out[9]  = (2.f * (qy * qz - qw * qx))        * sz;
        out[10] = (1.f - 2.f * (qx * qx + qy * qy)) * sz;
        out[11] = 0.f;

        // Column 3 (translation)
        out[12] = Position.x;
        out[13] = Position.y;
        out[14] = Position.z;
        out[15] = 1.f;
    }
};

} // namespace ImFrame::Rendering
