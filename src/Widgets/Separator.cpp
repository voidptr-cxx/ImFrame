/**
 * @file     Separator.cpp
 * @brief    Implementation of Widgets::Separator::Show()
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/Separator.hpp"

#include <imgui.h>

#include <string>

namespace ImFrame::Widgets {

bool Separator::Show() {
    if (_disabled) { ImGui::BeginDisabled(); }

    if (_label.empty()) {
        ImGui::Separator();
    } else {
        // SeparatorText requires a null-terminated string.
        std::string text(_label.data(), _label.size());
        ImGui::SeparatorText(text.c_str());
    }

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    return false;
}

} // namespace ImFrame::Widgets
