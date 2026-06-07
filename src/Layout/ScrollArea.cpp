/**
 * @file     ScrollArea.cpp
 * @brief    Implementation of ScrollArea::Begin() and scroll helpers
 *
 * @internal
 * ImGui headers are confined to this translation unit. Scroll queries
 * (`GetScrollX/Y`) and programmatic scroll (`SetScrollX/Y`, `SetScrollHereY`)
 * operate on the current window context, which is the child window after
 * `BeginChild()` has been called and before the corresponding `EndChild()`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Layout/ScrollArea.hpp"

#include <imgui.h>

#include <cstdio>

namespace ImFrame::Layout {

ChildScope ScrollArea::Begin() {
    char idBuf[256];
    std::snprintf(idBuf, sizeof(idBuf), "%.*s",
                  static_cast<int>(_id.size()), _id.data());

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
    if (_hBar) windowFlags |= ImGuiWindowFlags_HorizontalScrollbar;

    // Vertical scroll bar is the default; suppress it only when explicitly disabled.
    if (!_vBar) windowFlags |= ImGuiWindowFlags_NoScrollbar;

    const bool visible = ImGui::BeginChild(idBuf, ImVec2{_size.x, _size.y},
                                           ImGuiChildFlags_None, windowFlags);
    return ChildScope(visible);
}

void ScrollArea::ScrollToBottom() {
    ImGui::SetScrollHereY(1.0f);
}

Widgets::Vec2 ScrollArea::ScrollPosition() const {
    return {ImGui::GetScrollX(), ImGui::GetScrollY()};
}

void ScrollArea::SetScrollPosition(Widgets::Vec2 pos) {
    ImGui::SetScrollX(pos.x);
    ImGui::SetScrollY(pos.y);
}

} // namespace ImFrame::Layout
