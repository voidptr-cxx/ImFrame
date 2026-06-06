/**
 * @file     Theme.cpp
 * @brief    Theme::Apply() — translates semantic design tokens to ImGui style slots
 *
 * @internal
 * This is the only translation unit in the theme system that includes <imgui.h>.
 * Apply() performs ~63 direct assignments to ImGui::GetStyle() — no loops, no
 * maps, no runtime branches. A static_assert guards against silent coverage gaps
 * if Dear ImGui adds new ImGuiCol_* entries in a future upgrade.
 *
 * Alpha-modified tokens (e.g. TitleBgCollapsed, TextSelectedBg) are expressed as
 * inline ImVec4 constructions that override only the alpha channel; the r/g/b
 * channels come directly from the semantic token fields.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-04
 * @version  0.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Theme/Theme.hpp"

#include <imgui.h>

namespace ImFrame::Theme {

void Theme::Apply() const {
    // Verify that Apply() covers every ImGuiCol_* slot. If Dear ImGui adds a
    // new colour in a future update this assert fires — update the assignments
    // below before re-enabling the build.
    static_assert(ImGuiCol_COUNT == 63,
        "ImGuiCol_COUNT changed — update Theme::Apply() to cover all new colour slots");

    ImGuiStyle& s = ImGui::GetStyle();

    // ── Shorthand lambdas ─────────────────────────────────────────────────────
    // Build an ImVec4 from a ColorToken (full alpha)
    auto C = [](const ColorToken& t) -> ImVec4 {
        return {t.r, t.g, t.b, t.a};
    };
    // Build an ImVec4 from a ColorToken with an alpha override
    auto CA = [](const ColorToken& t, float a) -> ImVec4 {
        return {t.r, t.g, t.b, a};
    };

    // ── Window backgrounds ────────────────────────────────────────────────────
    s.Colors[ImGuiCol_WindowBg]          = C(backgroundPrimary);
    s.Colors[ImGuiCol_ChildBg]           = C(backgroundSecondary);
    s.Colors[ImGuiCol_PopupBg]           = C(backgroundSecondary);
    s.Colors[ImGuiCol_MenuBarBg]         = C(backgroundSecondary);

    // ── Title bar ─────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_TitleBg]           = C(backgroundTertiary);
    s.Colors[ImGuiCol_TitleBgActive]     = C(accentDefault);
    s.Colors[ImGuiCol_TitleBgCollapsed]  = CA(backgroundTertiary, 0.75f);

    // ── Text ──────────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_Text]              = C(textPrimary);
    s.Colors[ImGuiCol_TextDisabled]      = C(textDisabled);
    s.Colors[ImGuiCol_TextLink]          = C(accentDefault);
    s.Colors[ImGuiCol_TextSelectedBg]    = CA(accentDefault, 0.35f);

    // ── Borders ───────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_Border]            = C(borderDefault);
    s.Colors[ImGuiCol_BorderShadow]      = {0.0f, 0.0f, 0.0f, 0.0f};

    // ── Frames (InputText, Slider, ColorEdit…) ────────────────────────────────
    s.Colors[ImGuiCol_FrameBg]           = C(surfaceDefault);
    s.Colors[ImGuiCol_FrameBgHovered]    = C(surfaceHover);
    s.Colors[ImGuiCol_FrameBgActive]     = C(surfaceActive);

    // ── Scrollbar ─────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_ScrollbarBg]       = C(backgroundPrimary);
    s.Colors[ImGuiCol_ScrollbarGrab]     = C(surfaceDefault);
    s.Colors[ImGuiCol_ScrollbarGrabHovered] = C(surfaceHover);
    s.Colors[ImGuiCol_ScrollbarGrabActive]  = C(accentDefault);

    // ── Check / Slider ────────────────────────────────────────────────────────
    // Note: checkbox default/hovered backgrounds use FrameBg/FrameBgHovered.
    s.Colors[ImGuiCol_CheckMark]         = C(accentDefault);
    s.Colors[ImGuiCol_CheckboxSelectedBg]= C(accentDefault);
    s.Colors[ImGuiCol_SliderGrab]        = C(accentDefault);
    s.Colors[ImGuiCol_SliderGrabActive]  = C(accentActive);

    // ── Buttons ───────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_Button]            = C(surfaceDefault);
    s.Colors[ImGuiCol_ButtonHovered]     = C(surfaceHover);
    s.Colors[ImGuiCol_ButtonActive]      = C(surfaceActive);

    // ── Headers (CollapsingHeader, TreeNode, Selectable…) ────────────────────
    s.Colors[ImGuiCol_Header]            = C(surfaceDefault);
    s.Colors[ImGuiCol_HeaderHovered]     = C(surfaceHover);
    s.Colors[ImGuiCol_HeaderActive]      = C(surfaceActive);

    // ── Separator ─────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_Separator]         = C(borderDefault);
    s.Colors[ImGuiCol_SeparatorHovered]  = C(borderHover);
    s.Colors[ImGuiCol_SeparatorActive]   = C(accentDefault);

    // ── Resize grip ───────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_ResizeGrip]        = C(surfaceDefault);
    s.Colors[ImGuiCol_ResizeGripHovered] = C(accentDefault);
    s.Colors[ImGuiCol_ResizeGripActive]  = C(accentActive);

    // ── InputText cursor ──────────────────────────────────────────────────────
    s.Colors[ImGuiCol_InputTextCursor]   = C(textPrimary);

    // ── Tabs ──────────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_Tab]                         = C(backgroundTertiary);
    s.Colors[ImGuiCol_TabHovered]                  = C(surfaceHover);
    s.Colors[ImGuiCol_TabSelected]                 = C(backgroundPrimary);
    s.Colors[ImGuiCol_TabSelectedOverline]         = C(accentDefault);
    s.Colors[ImGuiCol_TabDimmed]                   = C(backgroundTertiary);
    s.Colors[ImGuiCol_TabDimmedSelected]           = C(backgroundSecondary);
    s.Colors[ImGuiCol_TabDimmedSelectedOverline]   = CA(accentDefault, 0.5f);

    // ── Docking ───────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_DockingPreview]    = CA(accentDefault, 0.7f);
    s.Colors[ImGuiCol_DockingEmptyBg]    = C(backgroundPrimary);

    // ── Plots ─────────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_PlotLines]         = C(accentDefault);
    s.Colors[ImGuiCol_PlotLinesHovered]  = C(accentHover);
    s.Colors[ImGuiCol_PlotHistogram]     = C(accentDefault);
    s.Colors[ImGuiCol_PlotHistogramHovered] = C(accentHover);

    // ── Tables ────────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_TableHeaderBg]     = C(backgroundTertiary);
    s.Colors[ImGuiCol_TableBorderStrong] = C(borderDefault);
    s.Colors[ImGuiCol_TableBorderLight]  = CA(borderDefault, 0.5f);
    s.Colors[ImGuiCol_TableRowBg]        = {0.0f, 0.0f, 0.0f, 0.0f};
    s.Colors[ImGuiCol_TableRowBgAlt]     = CA(textPrimary, 0.06f);

    // ── Tree lines ────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_TreeLines]         = C(borderDefault);

    // ── Drag & Drop ───────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_DragDropTarget]    = C(accentDefault);
    s.Colors[ImGuiCol_DragDropTargetBg]  = CA(accentDefault, 0.25f);

    // ── Misc ──────────────────────────────────────────────────────────────────
    s.Colors[ImGuiCol_UnsavedMarker]           = C(statusWarning);
    s.Colors[ImGuiCol_NavCursor]               = C(accentDefault);
    s.Colors[ImGuiCol_NavWindowingHighlight]   = CA(accentDefault, 0.7f);
    s.Colors[ImGuiCol_NavWindowingDimBg]       = CA(backgroundPrimary, 0.8f);
    s.Colors[ImGuiCol_ModalWindowDimBg]        = CA(backgroundPrimary, 0.8f);

    // ── Spacing ───────────────────────────────────────────────────────────────
    s.ItemSpacing   = {spacing.ItemSpacingX,   spacing.ItemSpacingY};
    s.WindowPadding = {spacing.WindowPaddingX, spacing.WindowPaddingY};
    s.FramePadding  = {spacing.FramePaddingX,  spacing.FramePaddingY};
    s.IndentSpacing =  spacing.IndentSpacing;

    // ── Rounding ──────────────────────────────────────────────────────────────
    s.WindowRounding    = radius.Window;
    s.FrameRounding     = radius.Frame;
    s.PopupRounding     = radius.Popup;
    s.ScrollbarRounding = radius.Scrollbar;
}

} // namespace ImFrame::Theme
