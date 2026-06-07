/**
 * @file     LayoutImpl.cpp
 * @brief    Non-template ImGui helpers backing HStack, VStack, and Grid Render() methods
 *
 * @internal
 * These functions are declared in the respective public headers (in
 * `namespace ImFrame::Internal`) so that template `Render()` methods can call
 * them without including `<imgui.h>`. Definitions live here, keeping ImGui
 * confined to a single translation unit per layout type.
 *
 * HStack::HStackSameLine
 *   Calls `ImGui::SameLine(0, spacing)`. A negative spacing defers to ImGui's
 *   default item spacing.
 *
 * VStack::VStackDummy
 *   Emits an invisible `ImGui::Dummy({0, spacing})` for vertical gap.
 *
 * Grid::GridBeginTable / GridNextColumn / GridEndTable
 *   Thin wrappers around the ImGui table API. `GridBeginTable` pushes per-cell
 *   padding when `cellPadding > 0`. `GridEndTable` must only be called when
 *   `GridBeginTable` returned true.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Layout/Grid.hpp"
#include "ImFrame/Layout/HStack.hpp"
#include "ImFrame/Layout/VStack.hpp"

#include <imgui.h>

#include <cstdio>

namespace ImFrame::Internal {

// ─── HStack ───────────────────────────────────────────────────────────────────

void HStackSameLine(float spacing) {
    // SameLine(offset_from_start_x=0, spacing_w) — offset 0 means "right after prev item".
    ImGui::SameLine(0.0f, spacing < 0.0f ? -1.0f : spacing);
}

// ─── VStack ───────────────────────────────────────────────────────────────────

void VStackDummy(float spacing) {
    ImGui::Dummy(ImVec2{0.0f, spacing});
}

// ─── Grid ─────────────────────────────────────────────────────────────────────

bool GridBeginTable(std::string_view id, int columns, float cellPadding) {
    char idBuf[256];
    std::snprintf(idBuf, sizeof(idBuf), "%.*s",
                  static_cast<int>(id.size()), id.data());

    if (cellPadding > 0.0f) {
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2{cellPadding, cellPadding});
    }

    const bool ok = ImGui::BeginTable(idBuf, columns, ImGuiTableFlags_None);

    if (cellPadding > 0.0f) {
        ImGui::PopStyleVar();
    }

    return ok;
}

void GridNextColumn() {
    ImGui::TableNextColumn();
}

void GridEndTable() {
    ImGui::EndTable();
}

} // namespace ImFrame::Internal
