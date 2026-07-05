/**
 * @file     VirtualList.hpp
 * @brief    Virtualised scrolling list rendering only visible items
 *
 * `VirtualList` renders only the items whose row falls within the current
 * scroll viewport, regardless of how large `itemCount` is — the per-index
 * `builder` `Delegate` fires only for those visible indices. Item height is
 * fixed (`itemHeight` applies uniformly to every row); variable-height items
 * are a known limitation of this phase (would require a two-pass layout).
 *
 * **Mechanism**
 * `Internal::VirtualListElement` opens its own ImGui scrolling child region in
 * `Paint()`, reads the live `ImGui::GetScrollY()`/`GetWindowHeight()`, and
 * computes the visible index range `[floor(scrollY/itemHeight),
 * ceil((scrollY+viewportHeight)/itemHeight)]`. Only those indices get a real
 * child `Element` (cached by index in an `unordered_map`, mounted the first
 * time an index becomes visible, updated while it stays visible, unmounted
 * the frame it scrolls back out of view). A trailing zero-size `ImGui::Dummy`
 * at `itemCount * itemHeight` reserves the full scroll extent so the scrollbar
 * reflects the true (unvirtualized) content size.
 *
 * `Table` (Phase 14) is reimplemented on top of `VirtualList` in this phase,
 * replacing its direct `ImGuiListClipper` usage with this ImGui-internals-free
 * equivalent.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"

namespace ImFrame::Tree {

/**
 * @class    VirtualList
 * @brief    Fixed-row-height virtualised list — only visible rows are built
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * VirtualList(10000, 24.0f, [](int index) -> Widget {
 *     return Text(std::format("Row {}", index));
 * });
 * @endcode
 */
class VirtualList {
public:
    /**
     * @brief    Constructs a fixed-height virtualised list.
     * @param[in] itemCount   Total logical row count.
     * @param[in] itemHeight  Uniform row height in pixels.
     * @param[in] builder     Called only for the currently-visible row indices; returns that row's `Widget`.
     */
    VirtualList(int itemCount, float itemHeight, Utility::Delegate<Widget(int)> builder)
        : _itemCount(itemCount)
        , _itemHeight(itemHeight)
        , _builder(std::move(builder)) {}

    /// Explicit identity override — see `Tree::Key`.
    VirtualList& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] int       GetItemCount() const noexcept { return _itemCount; }
    [[nodiscard]] float     GetItemHeight() const noexcept { return _itemHeight; }

    [[nodiscard]] const Utility::Delegate<Widget(int)>& GetBuilder() const noexcept { return _builder; }

    /// @internal Produces this list's concrete `Element`. Defined in `VirtualListRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    int                            _itemCount  = 0;
    float                          _itemHeight = 0.0f;
    Utility::Delegate<Widget(int)> _builder;
    Tree::Key                      _key;
};

} // namespace ImFrame::Tree
