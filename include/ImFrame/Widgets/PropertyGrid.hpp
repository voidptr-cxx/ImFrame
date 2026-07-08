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

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <string>
#include <string_view>
#include <vector>

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
/// @deprecated Use `PropertyGridWidget` instead (Phase 30). See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.2.
class [[deprecated("Use PropertyGridWidget instead. See Docs/Migration_v1_to_v2.md.")]] PropertyGrid {
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

// ─── PropertyGridWidget (Phase 30) ──────────────────────────────────────────────

/**
 * @struct   PropertyGridRow
 * @brief    One row of a `PropertyGridWidget` — a label/widget pair, or a group separator
 *
 * @since    2.5.0
 */
struct PropertyGridRow {
    std::string  label;               ///< Ignored when `isSeparator` is `true`.
    Tree::Widget widget;               ///< Ignored when `isSeparator` is `true`.
    bool         isSeparator = false;  ///< `true` renders `label` as a group header via `SeparatorWidget`.

    PropertyGridRow(std::string rowLabel, Tree::Widget rowWidget)
        : label(std::move(rowLabel)), widget(std::move(rowWidget)) {}

    /// Constructs a group-separator row (`label` becomes the section header text).
    [[nodiscard]] static PropertyGridRow Separator(std::string groupName);

private:
    /// @internal Separator-row tag constructor. Defined in `PropertyGrid.cpp` (needs `Tree::Primitives::Text`).
    PropertyGridRow(std::string groupName, bool);
};

/**
 * @class    PropertyGridWidget
 * @brief    Declarative two-column property layout — `Tree::Component` replacement for `PropertyGrid`
 *
 * The imperative `Begin()`/`Row()`/`Separator()` scope pattern has no
 * declarative analogue — `Build()` runs once per frame and returns a
 * complete tree up front, rather than being called into row-by-row. Instead,
 * callers supply the full row list via `Rows()`.
 *
 * `SplitRatio` can't be resolved to a fixed pixel width inside `Build()`
 * (which runs before any `Layout()` pass has constraint information), so each
 * row is composed as `Flex(Horizontal){ Expanded(Text(label), factor),
 * Expanded(rowWidget, 100-factor) }` — reusing `Flex`'s existing
 * `DistributeFlexFactors` proportional-width math instead of a fixed size.
 * This is pure composition: no new primitive or `Element` is written for
 * this widget, matching how `TableWidget` was composed in Phase 29.
 *
 * @since    2.5.0
 *
 * @example
 * @code
 * PropertyGridWidget("##props")
 *     .SplitRatio(0.4f)
 *     .Rows({
 *         {"Name", Widget(TextInputWidget("##name", &name))},
 *         PropertyGridRow::Separator("Transform"),
 *         {"Position", Widget(SliderWidget<float>("##x", &posX, -10.0f, 10.0f))},
 *     });
 * @endcode
 */
class PropertyGridWidget {
public:
    explicit PropertyGridWidget(std::string id) : _id(std::move(id)) {}

    PropertyGridWidget& SplitRatio(float ratio) noexcept { _splitRatio = ratio; return *this; }
    PropertyGridWidget& Rows(std::vector<PropertyGridRow> rows) { _rows = std::move(rows); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    PropertyGridWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetId() const noexcept { return _id; }
    [[nodiscard]] float GetSplitRatio() const noexcept { return _splitRatio; }
    [[nodiscard]] const std::vector<PropertyGridRow>& GetRows() const noexcept { return _rows; }

    /// @internal Composes the row tree. Defined in `PropertyGrid.cpp`. Satisfies `Tree::Component`.
    [[nodiscard]] Tree::Widget Build() const;

private:
    std::string                   _id;
    float                         _splitRatio = 0.35f;
    std::vector<PropertyGridRow>  _rows;
    Tree::Key                     _key;
};

} // namespace ImFrame::Widgets
