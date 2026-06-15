/**
 * @file     PropertyGrid.cpp
 * @brief    Implementation of Widgets::PropertyGrid, PropertyGridScope, and Internal helpers
 *
 * @internal
 * ImGui headers are confined to this translation unit.  The public header
 * (PropertyGrid.hpp) forward-declares the Internal helpers so that the
 * template `Row()` method can call them without including <imgui.h>.
 *
 * The grid is a two-column ImGui table with stretch-weighted columns.
 * PropertyGridBeginRow: TableNextRow → label column (AlignTextToFramePadding +
 *   TextUnformatted) → TableNextColumn (value column) → SetNextItemWidth(-FLT_MIN).
 * PropertyGridEndRow: no-op — next TableNextRow() advances automatically.
 * PropertyGridSeparator: renders a full-row header-coloured row with optional label.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-09
 * @version  1.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/PropertyGrid.hpp"

#include <imgui.h>

#include <cfloat>

namespace ImFrame::Internal {

void PropertyGridBeginRow(std::string_view label) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::TableSetColumnIndex(1);
    // Make the widget fill the entire value column.
    ImGui::SetNextItemWidth(-FLT_MIN);
}

void PropertyGridEndRow() {
    // No-op: ImGui advances rows automatically on the next TableNextRow() call.
}

void PropertyGridSeparator(std::string_view groupName) {
    ImGui::TableNextRow();
    // Tint the entire row with the table header background colour.
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                           ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    if (!groupName.empty()) {
        ImGui::TextUnformatted(groupName.data(), groupName.data() + groupName.size());
    }
}

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

// ─── PropertyGridScope ────────────────────────────────────────────────────────

PropertyGridScope::PropertyGridScope(bool open) : _open(open) {}

PropertyGridScope::PropertyGridScope(PropertyGridScope&& other) noexcept
    : _open(other._open) {
    other._open = false;
}

PropertyGridScope::~PropertyGridScope() noexcept {
    if (_open) ImGui::EndTable();
}

PropertyGridScope& PropertyGridScope::Separator(std::string_view groupName) {
    if (_open) Internal::PropertyGridSeparator(groupName);
    return *this;
}

// ─── PropertyGrid ─────────────────────────────────────────────────────────────

PropertyGrid::PropertyGrid(std::string_view id) : _id(id) {}

PropertyGrid& PropertyGrid::SplitRatio(float ratio) {
    _splitRatio = ratio;
    return *this;
}

PropertyGridScope PropertyGrid::Begin() {
    constexpr int flags = ImGuiTableFlags_SizingStretchProp
                        | ImGuiTableFlags_BordersInnerV;

    if (!ImGui::BeginTable(_id.c_str(), 2, flags)) {
        return PropertyGridScope(false);
    }

    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, _splitRatio);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch,
                            1.0f - _splitRatio);

    return PropertyGridScope(true);
}

} // namespace ImFrame::Widgets
