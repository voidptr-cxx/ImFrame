/**
 * @file     ColorEdit.cpp
 * @brief    Implementation of Widgets::ColorEdit::Show()
 *
 * @internal
 * Converts `Vec4` ↔ `ImVec4` by copying the four float fields. The
 * layout-identity static_assert guards against ABI drift.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/ColorEdit.hpp"
#include "WidgetHelpers.hpp"

#include <imgui.h>

static_assert(sizeof(ImFrame::Widgets::Vec4) == sizeof(ImVec4),
              "Widgets::Vec4 must be layout-identical to ImVec4");

namespace ImFrame::Widgets {

bool ColorEdit::Show() {
    char buf[256];
    Internal::BuildLabelBuf(buf, sizeof(buf), _label, _id);

    ImVec4 color{_value.x, _value.y, _value.z, _value.w};

    if (_width > 0.0f) { ImGui::SetNextItemWidth(_width); }
    if (_disabled)     { ImGui::BeginDisabled(); }

    bool changed = false;
    if (_alpha) {
        changed = ImGui::ColorEdit4(buf, reinterpret_cast<float*>(&color));
    } else {
        changed = ImGui::ColorEdit3(buf, reinterpret_cast<float*>(&color));
    }

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    if (changed) {
        _value = {color.x, color.y, color.z, color.w};
        if (_onChange) { _onChange(_value); }
    }
    return changed;
}

} // namespace ImFrame::Widgets
