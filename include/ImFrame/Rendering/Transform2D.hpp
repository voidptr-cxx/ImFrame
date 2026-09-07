/**
 * @file     Transform2D.hpp
 * @brief    2D affine transform stored as a 2×3 row-major floating-point matrix
 *
 * All matrix math is constexpr and header-only. `Compose` follows column-vector
 * convention: `A.Compose(B)` applies B first, then A — matching OpenGL/ImGui
 * coordinate pipeline ordering.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-28
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Widgets/Types.hpp"

#include <cmath>

namespace ImFrame::Rendering {

/**
 * @struct Transform2D
 * @brief  2D affine transform: x' = a*x + b*y + tx,  y' = c*x + d*y + ty
 *
 * Six-float 2×3 row-major layout. The implicit third row is [0 0 1].
 *
 * @since  2.0.0
 *
 * @example
 * @code
 * auto t = Transform2D::Translation({10.f, 0.f}).Compose(Transform2D::Scale({2.f, 2.f}));
 * auto p = t.Apply({5.f, 5.f}); // scale first → {10,10}, then translate → {20,10}
 * @endcode
 */
struct Transform2D {
    float a  = 1.0f; ///< X-basis X-component (cosine of rotation, or X-scale).
    float b  = 0.0f; ///< X-basis Y-component (sine of rotation or shear).
    float c  = 0.0f; ///< Y-basis X-component (negative sine of rotation or shear).
    float d  = 1.0f; ///< Y-basis Y-component (cosine of rotation, or Y-scale).
    float tx = 0.0f; ///< X translation.
    float ty = 0.0f; ///< Y translation.

    // ─── Static factories ──────────────────────────────────────────────────────

    /// Returns the identity transform (no translation, scale, or rotation).
    [[nodiscard]] static constexpr Transform2D Identity() noexcept {
        return {1.f, 0.f, 0.f, 1.f, 0.f, 0.f};
    }

    /**
     * @brief  Create a translation-only transform.
     * @param[in] offset  Translation amount in the target coordinate space.
     * @return  Translation transform.
     */
    [[nodiscard]] static constexpr Transform2D Translation(Widgets::Vec2 offset) noexcept {
        return {1.f, 0.f, 0.f, 1.f, offset.x, offset.y};
    }

    /**
     * @brief  Create an axis-aligned scale transform.
     * @param[in] scale  Per-axis scale factors.
     * @return  Scale transform.
     */
    [[nodiscard]] static constexpr Transform2D Scale(Widgets::Vec2 scale) noexcept {
        return {scale.x, 0.f, 0.f, scale.y, 0.f, 0.f};
    }

    /**
     * @brief  Create a counter-clockwise rotation transform around the origin.
     * @param[in] radians  Rotation angle (counter-clockwise positive).
     * @return  Rotation transform.
     */
    [[nodiscard]] static Transform2D Rotation(float radians) noexcept {
        const float cosA = std::cos(radians);
        const float sinA = std::sin(radians);
        return {cosA, -sinA, sinA, cosA, 0.f, 0.f};
    }

    // ─── Operations ────────────────────────────────────────────────────────────

    /**
     * @brief  Compose with another transform: applies `other` first, then `*this`.
     *
     * Equivalent to the matrix product `(*this) * other` in column-vector notation.
     *
     * @param[in] other  The transform to apply before `*this`.
     * @return  Combined transform.
     */
    [[nodiscard]] constexpr Transform2D Compose(const Transform2D& other) const noexcept {
        return {
            a * other.a + b * other.c,
            a * other.b + b * other.d,
            c * other.a + d * other.c,
            c * other.b + d * other.d,
            a * other.tx + b * other.ty + tx,
            c * other.tx + d * other.ty + ty
        };
    }

    /**
     * @brief  Apply this transform to a 2D point.
     * @param[in] p  Point in the source coordinate space.
     * @return  Transformed point.
     */
    [[nodiscard]] constexpr Widgets::Vec2 Apply(Widgets::Vec2 p) const noexcept {
        return {a * p.x + b * p.y + tx, c * p.x + d * p.y + ty};
    }
};

} // namespace ImFrame::Rendering
