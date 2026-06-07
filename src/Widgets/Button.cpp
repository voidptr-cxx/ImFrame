/**
 * @file     Button.cpp
 * @brief    Implementation of Widgets::Button::Show()
 *
 * @internal
 * ImGui headers are confined to this translation unit. The public header
 * (Button.hpp) contains no ImGui includes, preserving the architecture invariant.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/Button.hpp"
#include "WidgetHelpers.hpp"

#include <imgui.h>

#include <cstdio>
#include <cstring>

static_assert(sizeof(ImFrame::Widgets::Vec2) == sizeof(ImVec2),
              "Widgets::Vec2 must be layout-identical to ImVec2");

namespace ImFrame::Widgets {

bool Button::Show() {
    // Build label: "icon  label##id" (icon path) or "label##id" (plain path).
    char buf[256];
    if (_icon && _icon[0] != '\0') {
        char labelBuf[256];
        Internal::BuildLabelBuf(labelBuf, sizeof(labelBuf), _label, _id);
        // NBSP NBSP separator between glyph and label text.
        std::snprintf(buf, sizeof(buf), "%s\xc2\xa0\xc2\xa0%s", _icon, labelBuf);
    } else {
        Internal::BuildLabelBuf(buf, sizeof(buf), _label, _id);
    }

    if (_width > 0.0f) { ImGui::SetNextItemWidth(_width); }
    if (_disabled)     { ImGui::BeginDisabled(); }

    const ImVec2 sz{_size.x, _size.y};
    bool clicked = ImGui::Button(buf, sz);

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    if (clicked && _onClick) { _onClick(); }
    return clicked;
}

} // namespace ImFrame::Widgets
