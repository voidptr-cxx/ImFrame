/**
 * @file     ScrollArea.hpp
 * @brief    Scrollable child-window container with optional scroll bars
 *
 * `ScrollArea` wraps `ImGui::BeginChild` / `ImGui::EndChild` with scroll-bar
 * flags. Scroll position queries and programmatic scrolling are available as
 * member functions to be called from within the active child-window scope.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Layout/ChildScope.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <string_view>

namespace ImFrame::Layout {

/**
 * @class    ScrollArea
 * @brief    Scrollable region with configurable horizontal and vertical bars
 *
 * After calling `Begin()`, use `ScrollToBottom()`, `ScrollPosition()`, and
 * `SetScrollPosition()` from within the active child scope to query and control
 * the scroll offset of the current child window.
 *
 * @since    1.1.0
 *
 * @example
 * @code
 * auto area = ScrollArea("log_view").VerticalBar(true);
 * if (auto scope = area.Begin()) {
 *     for (auto& line : logLines) Widgets::Text(line).Show();
 *     if (autoScroll) area.ScrollToBottom();
 * }
 * @endcode
 */
/// @deprecated Use `Tree::VirtualList` for large scrolling lists. See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.
class [[deprecated("See Docs/Migration_v1_to_v2.md.")]] ScrollArea {
public:
    /**
     * @brief    Construct a scroll area with the given ImGui child-window ID.
     * @param[in]  id  Unique string ID passed to `ImGui::BeginChild()`.
     * @throws   Nothing.
     */
    explicit ScrollArea(std::string_view id) noexcept : _id(id) {}

    ScrollArea& Size(Widgets::Vec2 size) noexcept             { _size = size;              return *this; }
    ScrollArea& HorizontalBar(bool enabled = true) noexcept   { _hBar = enabled;           return *this; }
    ScrollArea& VerticalBar(bool enabled = true) noexcept     { _vBar = enabled;           return *this; }

    /**
     * @brief    Open the scrollable child window and return an RAII scope.
     * @return   `ChildScope` that evaluates as `true` when the window is visible.
     *           The scope's destructor always calls `ImGui::EndChild()`.
     * @throws   Nothing.
     */
    [[nodiscard]] ChildScope Begin();

    /**
     * @brief    Scroll to the bottom of the child window.
     *
     * Calls `ImGui::SetScrollHereY(1.0f)`. Must be called from within the active
     * child scope (after `Begin()` has returned a truthy scope).
     *
     * @throws   Nothing.
     */
    void ScrollToBottom();

    /**
     * @brief    Return the current scroll position of the child window.
     *
     * Must be called from within the active child scope.
     *
     * @return   Scroll position as `{x, y}` in pixels.
     * @throws   Nothing.
     */
    [[nodiscard]] Widgets::Vec2 ScrollPosition() const;

    /**
     * @brief    Set the scroll position programmatically.
     *
     * Calls `ImGui::SetScrollX` and `ImGui::SetScrollY`. Must be called from
     * within the active child scope.
     *
     * @param[in]  pos  Target scroll position in pixels.
     * @throws   Nothing.
     */
    void SetScrollPosition(Widgets::Vec2 pos);

private:
    std::string_view _id;
    Widgets::Vec2    _size {};
    bool             _hBar = false;
    bool             _vBar = true;
};

} // namespace ImFrame::Layout
