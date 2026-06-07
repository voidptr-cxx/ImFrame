/**
 * @file     Panel.cpp
 * @brief    Implementation of Panel::Begin()
 *
 * @internal
 * ImGui headers are confined to this translation unit. Style overrides for
 * padding and background are pushed before `BeginChild()` and popped immediately
 * after, which is the correct sequence for child-window style vars.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Layout/Panel.hpp"

#include <imgui.h>

#include <cstdio>

namespace ImFrame::Layout {

ChildScope Panel::Begin() {
    char idBuf[256];
    std::snprintf(idBuf, sizeof(idBuf), "%.*s",
                  static_cast<int>(_id.size()), _id.data());

    if (_hasPadding) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{_padding.x, _padding.y});
    }
    if (_hasBg) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{_bg.x, _bg.y, _bg.z, _bg.w});
    }

    const ImGuiChildFlags childFlags = _border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
    const bool visible = ImGui::BeginChild(idBuf, ImVec2{_size.x, _size.y}, childFlags);

    if (_hasBg)      { ImGui::PopStyleColor(); }
    if (_hasPadding) { ImGui::PopStyleVar(); }

    return ChildScope(visible);
}

} // namespace ImFrame::Layout
