/**
 * @file     PropertyGrid.hpp
 * @brief    Two-column label/widget layout for inspector-style property editing
 *
 * `PropertyGrid` wraps an ImGui two-column table to render a list of
 * label/widget pairs with a configurable label-to-value split ratio.
 * Group headers are inserted with `PropertyGridScope::Separator()`.
 *
 * Usage pattern mirrors `Panel`/`ChildScope`: call `Begin()` to get a
 * `PropertyGridScope`, then call `Row()` and `Separator()` on the scope.
 * The scope destructor closes the underlying table when it goes out of scope.
 *
 * `Row()` is a template constrained by `Widgets::Renderable` so that any
 * ImFrame widget can be passed directly without wrapping.  The ImGui table
 * calls are hidden behind `ImFrame::Internal` helpers so that `<imgui.h>`
 * is never pulled into this public header.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-09
 * @version  1.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Widgets/Types.hpp"

#include <string>
#include <string_view>

// ─── Internal helpers (declared here for template Row() — not public API) ─────
namespace ImFrame::Internal {

/** @brief  Advance to the next row and render the label in the left column. */
void PropertyGridBeginRow(std::string_view label);

/** @brief  Finalise the current row after the right-column widget has rendered. */
void PropertyGridEndRow();

/** @brief  Render a visually distinct separator row with an optional header label. */
void PropertyGridSeparator(std::string_view groupName);

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

// ─── PropertyGridScope ────────────────────────────────────────────────────────

/**
 * @class    PropertyGridScope
 * @brief    RAII handle returned by `PropertyGrid::Begin()` — provides `Row()` and `Separator()`
 *
 * Moveable-only.  Calls `ImGui::EndTable()` in the destructor when the
 * underlying table opened successfully (`operator bool()` returns `true`).
 *
 * @since    1.4.0
 */
class PropertyGridScope {
public:
    ~PropertyGridScope() noexcept;

    PropertyGridScope(PropertyGridScope&&) noexcept;
    PropertyGridScope& operator=(PropertyGridScope&&) = delete;
    PropertyGridScope(const PropertyGridScope&)       = delete;
    PropertyGridScope& operator=(const PropertyGridScope&) = delete;

    /** @return `true` when the underlying ImGui table opened successfully. */
    explicit operator bool() const noexcept { return _open; }

    /**
     * @brief    Render a label/widget pair as one row in the property grid.
     *
     * The label is displayed in the left column; `widget.Show()` is called in
     * the right column.  Does nothing when the scope is not open.
     *
     * @tparam   TWidget  Any type satisfying `Widgets::Renderable` (has `Show()`).
     * @param[in]  label   Text shown in the left column.
     * @param[in,out]  widget  Widget rendered in the right column.
     * @return   `*this` for chaining.
     * @throws   Nothing.
     */
    template <Renderable TWidget>
    PropertyGridScope& Row(std::string_view label, TWidget& widget) {
        if (!_open) return *this;
        Internal::PropertyGridBeginRow(label);
        widget.Show();
        Internal::PropertyGridEndRow();
        return *this;
    }

    /**
     * @brief    Insert a visually distinct separator row, optionally labelled.
     * @param[in]  groupName  Header text for the group (empty for an unlabelled divider).
     * @return   `*this` for chaining.
     * @throws   Nothing.
     */
    PropertyGridScope& Separator(std::string_view groupName = {});

private:
    friend class PropertyGrid;
    explicit PropertyGridScope(bool open);
    bool _open;
};

// ─── PropertyGrid ─────────────────────────────────────────────────────────────

/**
 * @class    PropertyGrid
 * @brief    Inspector-style two-column label/value layout backed by an ImGui table
 *
 * @since    1.4.0
 *
 * @example
 * @code
 * PropertyGrid grid("##props");
 * grid.SplitRatio(0.4f);
 *
 * if (auto scope = grid.Begin()) {
 *     scope.Row("Name",     _nameInput)
 *          .Separator("Transform")
 *          .Row("Position", _posX)
 *          .Row("Rotation", _rotX);
 * }
 * @endcode
 */
/// @deprecated Phase 10–14 imperative widget API, not yet reimplemented as a Tree Component. See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.
class [[deprecated("See Docs/Migration_v1_to_v2.md.")]] PropertyGrid {
public:
    /**
     * @brief    Construct a property grid with a unique identifier.
     * @param[in]  id  Unique ImGui identifier (e.g., `"##props"`).
     * @throws   Nothing.
     */
    explicit PropertyGrid(std::string_view id);

    /**
     * @brief    Set the fractional width of the label column.
     * @param[in]  ratio  Label column fraction of total width (default `0.35f`).
     *                    The value column takes `1.0f - ratio`.
     * @return   `*this` for chaining.
     */
    PropertyGrid& SplitRatio(float ratio);

    /**
     * @brief    Open the underlying two-column ImGui table and return a scoped handle.
     *
     * @return   `PropertyGridScope` — check `operator bool()` before adding rows.
     * @throws   Nothing.
     */
    [[nodiscard]] PropertyGridScope Begin();

private:
    std::string      _id;
    float            _splitRatio = 0.35f;
};

} // namespace ImFrame::Widgets
