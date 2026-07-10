/**
 * @file     PropertyGrid.hpp
 * @brief    Two-column label/widget layout for inspector-style property editing
 *
 * `PropertyGridWidget` is a `Tree::Component`: callers compose the full row
 * list up front via `Rows()`, rather than being driven row-by-row through an
 * imperative scope.
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
#include <vector>

namespace ImFrame::Widgets {

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
 *         {"Name", TextInputWidget("##name", &name)},
 *         PropertyGridRow::Separator("Transform"),
 *         {"Position", SliderWidget<float>("##x", &posX, -10.0f, 10.0f)},
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
