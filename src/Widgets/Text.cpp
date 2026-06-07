/**
 * @file     Text.cpp
 * @brief    Implementation of Widgets::Text::Show()
 *
 * @internal
 * Priority for rendering mode: Disabled > Colored > Wrapped > plain.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/Text.hpp"

#include <imgui.h>

namespace ImFrame::Widgets {

bool Text::Show() {
    const char* begin = _text.data();
    const char* end   = begin + _text.size();

    if (_disabled) {
        ImGui::TextDisabled("%.*s", static_cast<int>(_text.size()), _text.data());
    } else if (_colored) {
        const ImVec4 col{_color.x, _color.y, _color.z, _color.w};
        ImGui::TextColored(col, "%.*s", static_cast<int>(_text.size()), _text.data());
    } else if (_wrapped) {
        ImGui::TextWrapped("%.*s", static_cast<int>(_text.size()), _text.data());
    } else {
        ImGui::TextUnformatted(begin, end);
    }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    return false;
}

} // namespace ImFrame::Widgets
