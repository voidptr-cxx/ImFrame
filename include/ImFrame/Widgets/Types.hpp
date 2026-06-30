/**
 * @file     Types.hpp
 * @brief    Shared value types and the Renderable concept for the Widgets subsystem
 *
 * Defines `Vec2`, `Vec4`, and `TextureHandle` as ImGui-free alternatives to
 * `ImVec2`, `ImVec4`, and `ImTextureID`. The types are layout-identical to their
 * ImGui counterparts; each widget .cpp verifies this with a `static_assert`.
 *
 * Introducing these types here also delivers the minimal Phase-2 core types
 * needed by the widget layer, since Phase 2 was intentionally deferred.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <concepts>

namespace ImFrame::Widgets {

// ─── Vec2 ─────────────────────────────────────────────────────────────────────

/**
 * @struct   Vec2
 * @brief    Two-component float vector, layout-identical to `ImVec2`
 *
 * Used in place of `ImVec2` in the public widget API to keep ImGui headers out
 * of `include/ImFrame/`. Widget `.cpp` files convert by copying the `x` and `y`
 * fields into a local `ImVec2`.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Button("OK").Size({120.0f, 0.0f}).Show();
 * @endcode
 */
struct Vec2 {
    float x = 0.0f; ///< Horizontal component.
    float y = 0.0f; ///< Vertical component.
};

// ─── Vec3 ─────────────────────────────────────────────────────────────────────

/**
 * @struct   Vec3
 * @brief    Three-component float vector for 3D positions, directions, and extents
 *
 * No equivalent exists in Dear ImGui (which provides only `ImVec2` and `ImVec4`).
 * Introduced in Phase 26 for the 3D viewport and gizmo subsystem.
 *
 * @since    2.1.0
 *
 * @example
 * @code
 * Widgets::Vec3 pos{1.0f, 2.0f, 3.0f};
 * @endcode
 */
struct Vec3 {
    float x = 0.0f; ///< X component.
    float y = 0.0f; ///< Y component.
    float z = 0.0f; ///< Z component.
};

// ─── Vec4 ─────────────────────────────────────────────────────────────────────

/**
 * @struct   Vec4
 * @brief    Four-component float vector, layout-identical to `ImVec4`
 *
 * Used in place of `ImVec4` for colour bindings and tint parameters in the
 * public widget API. All four fields are in `[0, 1]` normalised range for
 * colour usage; UVs or other four-float data may use any range.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Vec4 color{1.0f, 0.5f, 0.0f, 1.0f}; // RGBA orange
 * Widgets::ColorEdit("Tint", color).Show();
 * @endcode
 */
struct Vec4 {
    float x = 0.0f; ///< Red component (or X).
    float y = 0.0f; ///< Green component (or Y).
    float z = 0.0f; ///< Blue component (or Z).
    float w = 0.0f; ///< Alpha component (or W).
};

// ─── TextureHandle ────────────────────────────────────────────────────────────

/// Opaque GPU texture handle, matching the default `ImTextureID = void*`.
using TextureHandle = void*;

// ─── Renderable ───────────────────────────────────────────────────────────────

/**
 * @brief    Concept satisfied by any type exposing a `bool Show()` member
 *
 * Phase 11 layout containers use `Renderable` to constrain template parameters.
 * Every widget in this phase satisfies the concept.
 *
 * @since    1.0.0
 */
template<typename T>
concept Renderable = requires(T t) { { t.Show() } -> std::same_as<bool>; };

} // namespace ImFrame::Widgets
