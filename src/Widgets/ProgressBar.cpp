/**
 * @file     ProgressBar.cpp
 * @brief    Implementation of Widgets::ProgressBar::Show()
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

#include "ImFrame/Widgets/ProgressBar.hpp"
#include "WidgetHelpers.hpp"

#include <imgui.h>

#include <string>

namespace ImFrame::Widgets {

bool ProgressBar::Show() {
    if (_width > 0.0f) { ImGui::SetNextItemWidth(_width); }
    if (_disabled)     { ImGui::BeginDisabled(); }

    const ImVec2 sz{_size.x, _size.y};

    // Build a null-terminated overlay string (or nullptr for no overlay).
    std::string overlayStr;
    const char* overlayPtr = nullptr;
    if (!_overlay.empty()) {
        overlayStr.assign(_overlay.data(), _overlay.size());
        overlayPtr = overlayStr.c_str();
    }

    ImGui::ProgressBar(_fraction, sz, overlayPtr);

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    return false;
}

} // namespace ImFrame::Widgets
