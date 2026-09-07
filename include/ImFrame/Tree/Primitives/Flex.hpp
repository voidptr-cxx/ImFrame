/**
 * @file     Flex.hpp
 * @brief    Axis-aligned flexbox-style layout primitive
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"

#include <vector>

namespace ImFrame::Tree::Primitives {

/**
 * @class    Flex
 * @brief    Lays out children along a single axis, CSS-flexbox style
 *
 * Children that are `Expanded` or `Spacer` consume a share of the leftover
 * main-axis space proportional to their flex factor; all other children are
 * sized to their own intrinsic size.
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * Flex(Flex::Axis::Horizontal)
 *     .MainAlignment(Flex::MainAlignment::SpaceBetween)
 *     .Gap(8.0f)
 *     .Children({ Text("Left"), Spacer(), Text("Right") });
 * @endcode
 */
class Flex {
public:
    /// Layout axis.
    enum class Axis : unsigned char { Horizontal, Vertical };

    /// Main-axis distribution mode.
    enum class MainAlignment : unsigned char { Start, Center, End, SpaceBetween, SpaceAround };

    /// Cross-axis alignment mode.
    enum class CrossAlignment : unsigned char { Start, Center, End, Stretch };

    explicit Flex(Axis axis = Axis::Horizontal) : _axis(axis) {}

    Flex& AxisDirection(Axis axis) noexcept { _axis = axis; return *this; }
    Flex& MainAlign(MainAlignment align) noexcept { _mainAlign = align; return *this; }
    Flex& CrossAlign(CrossAlignment align) noexcept { _crossAlign = align; return *this; }
    Flex& Gap(float gap) noexcept { _gap = gap; return *this; }
    Flex& Children(std::vector<Widget> children) { _children = std::move(children); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    Flex& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }

    [[nodiscard]] Axis                  GetAxis() const noexcept { return _axis; }
    [[nodiscard]] MainAlignment         GetMainAlign() const noexcept { return _mainAlign; }
    [[nodiscard]] CrossAlignment        GetCrossAlign() const noexcept { return _crossAlign; }
    [[nodiscard]] float                 GetGap() const noexcept { return _gap; }
    [[nodiscard]] const std::vector<Widget>& GetChildren() const noexcept { return _children; }

    /// @internal Produces this flex's concrete `Element`. Defined in `FlexRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    Axis                 _axis;
    MainAlignment        _mainAlign  = MainAlignment::Start;
    CrossAlignment       _crossAlign = CrossAlignment::Start;
    float                _gap        = 0.0f;
    std::vector<Widget>  _children;
    Tree::Key            _key;
};

} // namespace ImFrame::Tree::Primitives
