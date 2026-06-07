/**
 * @file     Spacer.cpp
 * @brief    Implementation of Widgets::Spacer::Show()
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

#include "ImFrame/Widgets/Spacer.hpp"

#include <imgui.h>

namespace ImFrame::Widgets {

bool Spacer::Show() {
    if (_disabled) { ImGui::BeginDisabled(); }

    ImGui::Dummy(ImVec2{_size.x, _size.y});

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    return false;
}

} // namespace ImFrame::Widgets
