/**
 * @file     GestureRegion.hpp
 * @brief    Input-detection wrapper with no visual output of its own
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <optional>

namespace ImFrame::Tree::Primitives {

/**
 * @class    GestureRegion
 * @brief    Wraps a child widget with click/hover/drag/scroll detection
 *
 * All of Phase 10's `Button`, `TextInput`, `Slider`, etc. are reimplemented
 * as `GestureRegion` compositions starting in Phase 29.
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * GestureRegion()
 *     .OnClick([&] { Save(); })
 *     .Child(Box().Background({0.2f,0.2f,0.2f,1.0f}).Child(Text("Save")));
 * @endcode
 */
class GestureRegion {
public:
    GestureRegion() = default;

    GestureRegion& OnClick(Utility::Delegate<void()> cb) { _onClick = std::move(cb); return *this; }
    GestureRegion& OnDoubleClick(Utility::Delegate<void()> cb) { _onDoubleClick = std::move(cb); return *this; }
    GestureRegion& OnHover(Utility::Delegate<void(bool)> cb) { _onHover = std::move(cb); return *this; }
    GestureRegion& OnDragStart(Utility::Delegate<void()> cb) { _onDragStart = std::move(cb); return *this; }
    GestureRegion& OnDragMove(Utility::Delegate<void(Widgets::Vec2)> cb) { _onDragMove = std::move(cb); return *this; }
    GestureRegion& OnDragEnd(Utility::Delegate<void()> cb) { _onDragEnd = std::move(cb); return *this; }
    GestureRegion& OnScroll(Utility::Delegate<void(Widgets::Vec2)> cb) { _onScroll = std::move(cb); return *this; }

    GestureRegion& Child(Widget child) { _child.emplace(std::move(child)); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    GestureRegion& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }

    [[nodiscard]] const Utility::Delegate<void()>& GetOnClick() const noexcept { return _onClick; }
    [[nodiscard]] const Utility::Delegate<void()>& GetOnDoubleClick() const noexcept { return _onDoubleClick; }
    [[nodiscard]] const Utility::Delegate<void(bool)>& GetOnHover() const noexcept { return _onHover; }
    [[nodiscard]] const Utility::Delegate<void()>& GetOnDragStart() const noexcept { return _onDragStart; }
    [[nodiscard]] const Utility::Delegate<void(Widgets::Vec2)>& GetOnDragMove() const noexcept { return _onDragMove; }
    [[nodiscard]] const Utility::Delegate<void()>& GetOnDragEnd() const noexcept { return _onDragEnd; }
    [[nodiscard]] const Utility::Delegate<void(Widgets::Vec2)>& GetOnScroll() const noexcept { return _onScroll; }
    [[nodiscard]] const std::optional<Widget>& GetChild() const noexcept { return _child; }

    /// @internal Produces this region's concrete `Element`. Defined in `GestureRegionRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    Utility::Delegate<void()>              _onClick;
    Utility::Delegate<void()>              _onDoubleClick;
    Utility::Delegate<void(bool)>          _onHover;
    Utility::Delegate<void()>              _onDragStart;
    Utility::Delegate<void(Widgets::Vec2)> _onDragMove;
    Utility::Delegate<void()>              _onDragEnd;
    Utility::Delegate<void(Widgets::Vec2)> _onScroll;
    std::optional<Widget>                   _child;
    Tree::Key                               _key;
};

} // namespace ImFrame::Tree::Primitives
