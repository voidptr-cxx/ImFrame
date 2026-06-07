/**
 * @file     Checkbox.cpp
 * @brief    Implementation of Widgets::Checkbox::Show()
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

#include "ImFrame/Widgets/Checkbox.hpp"
#include "WidgetHelpers.hpp"

#include <imgui.h>

namespace ImFrame::Widgets {

bool Checkbox::Show() {
    char buf[256];
    Internal::BuildLabelBuf(buf, sizeof(buf), _label, _id);

    if (_width > 0.0f) { ImGui::SetNextItemWidth(_width); }
    if (_disabled)     { ImGui::BeginDisabled(); }

    bool changed = ImGui::Checkbox(buf, &_value);

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    if (changed && _onChange) { _onChange(_value); }
    return changed;
}

} // namespace ImFrame::Widgets
