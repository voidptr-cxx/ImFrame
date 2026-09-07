/**
 * @file     Ray3D.hpp
 * @brief    World-space ray with an origin and a normalized direction
 *
 * Used as the return type of `Viewport3D::Unproject()`. Intersect the ray
 * against scene geometry inside the `OnRender` callback to implement
 * mouse-picking for 3D objects.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-29
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Widgets/Types.hpp"

namespace ImFrame::Rendering {

/**
 * @struct Ray3D
 * @brief  World-space ray produced by `Viewport3D::Unproject()`
 *
 * @since  2.1.0
 *
 * @example
 * @code
 * Ray3D ray = viewport3d.Unproject(mousePos);
 * // Intersect ray.Origin + t * ray.Direction with scene geometry.
 * @endcode
 */
struct Ray3D {
    Widgets::Vec3 Origin;    ///< Ray origin in world space.
    Widgets::Vec3 Direction; ///< Normalized direction vector.
};

} // namespace ImFrame::Rendering
