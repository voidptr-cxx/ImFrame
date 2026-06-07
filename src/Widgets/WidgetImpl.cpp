/**
 * @file     WidgetImpl.cpp
 * @brief    Non-template ImGui helpers backing TextInput, Slider, and Combo Show() methods
 *
 * @internal
 * These functions are declared in the respective public headers (in
 * `namespace ImFrame::Internal`) so that the template `Show()` methods can call
 * them without including `<imgui.h>`. Definitions live here, keeping ImGui
 * confined to a single translation unit per widget.
 *
 * TextInput::ShowTextInputStr / ShowTextInputU8
 *   Copies the bound string into a fixed stack buffer (up to 1024 chars) or a
 *   heap vector for longer strings, calls ImGui::InputText*, then writes back.
 *
 * Slider::ShowSliderInt / ShowSliderFloat / ShowSliderDouble
 *   Thin wrappers around ImGui::SliderInt, SliderFloat, and SliderScalar
 *   (double variant) with shared disabled/tooltip/width handling.
 *
 * Combo::ShowComboImpl
 *   Receives pre-built display labels from Combo<T>::Show() and drives
 *   ImGui::BeginCombo / Selectable / EndCombo.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/Combo.hpp"
#include "ImFrame/Widgets/Slider.hpp"
#include "ImFrame/Widgets/TextInput.hpp"
#include "WidgetHelpers.hpp"

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

// ─── Static layout assert ─────────────────────────────────────────────────────
static_assert(sizeof(ImFrame::Widgets::Vec2) == sizeof(ImVec2));
static_assert(sizeof(ImFrame::Widgets::Vec4) == sizeof(ImVec4));

namespace {

/// Render tooltip for the last ImGui item.
void MaybeTooltip(std::string_view tip) {
    if (!tip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s", static_cast<int>(tip.size()), tip.data());
    }
}

/// Build a null-terminated InputText label/hint from a string_view.
void MakeNullTerm(char* buf, std::size_t sz, std::string_view sv) {
    std::snprintf(buf, sz, "%.*s", static_cast<int>(sv.size()), sv.data());
}

} // namespace

namespace ImFrame::Internal {

// ─── TextInput helpers ────────────────────────────────────────────────────────

bool ShowTextInputStr(std::string_view id, std::string_view label, std::string& value,
                      std::string_view hint, bool multiline, bool password,
                      bool disabled, std::string_view tooltip, float width) {
    char labelBuf[256];
    BuildLabelBuf(labelBuf, sizeof(labelBuf), label, id);

    // Choose between stack and heap buffer depending on string length.
    constexpr std::size_t STACK_CAP = 1024;
    char          stackBuf[STACK_CAP];
    std::vector<char> heapBuf;
    char*             buf;
    std::size_t       bufCap;

    if (value.size() + 1 <= STACK_CAP) {
        std::memcpy(stackBuf, value.data(), value.size());
        stackBuf[value.size()] = '\0';
        buf    = stackBuf;
        bufCap = STACK_CAP;
    } else {
        heapBuf.resize(value.size() + 1);
        std::memcpy(heapBuf.data(), value.data(), value.size());
        heapBuf[value.size()] = '\0';
        buf    = heapBuf.data();
        bufCap = heapBuf.size();
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
    if (password) { flags |= ImGuiInputTextFlags_Password; }

    if (width > 0.0f) { ImGui::SetNextItemWidth(width); }
    if (disabled)     { ImGui::BeginDisabled(); }

    bool changed = false;
    if (multiline) {
        changed = ImGui::InputTextMultiline(labelBuf, buf, bufCap,
                                             ImVec2(-1.0f, 100.0f), flags);
    } else if (!hint.empty()) {
        char hintBuf[256];
        MakeNullTerm(hintBuf, sizeof(hintBuf), hint);
        changed = ImGui::InputTextWithHint(labelBuf, hintBuf, buf, bufCap, flags);
    } else {
        changed = ImGui::InputText(labelBuf, buf, bufCap, flags);
    }

    if (disabled) { ImGui::EndDisabled(); }
    MaybeTooltip(tooltip);

    if (changed) { value = buf; }
    return changed;
}

bool ShowTextInputU8(std::string_view id, std::string_view label, std::u8string& value,
                     std::string_view hint, bool multiline, bool password,
                     bool disabled, std::string_view tooltip, float width) {
    // Convert u8string → char proxy → back on change.
    // char8_t and char have the same size; the reinterpret is safe for UTF-8 bytes.
    std::string proxy(reinterpret_cast<const char*>(value.data()), value.size());
    bool changed = ShowTextInputStr(id, label, proxy, hint, multiline, password,
                                    disabled, tooltip, width);
    if (changed) {
        value.assign(reinterpret_cast<const char8_t*>(proxy.data()), proxy.size());
    }
    return changed;
}

// ─── Slider helpers ───────────────────────────────────────────────────────────

bool ShowSliderInt(std::string_view id, std::string_view label,
                   int& value, int min, int max,
                   std::string_view format, bool disabled,
                   std::string_view tooltip, float width) {
    char labelBuf[256];
    BuildLabelBuf(labelBuf, sizeof(labelBuf), label, id);

    char fmtBuf[64];
    const char* fmt = nullptr;
    if (!format.empty()) {
        MakeNullTerm(fmtBuf, sizeof(fmtBuf), format);
        fmt = fmtBuf;
    }

    if (width > 0.0f) { ImGui::SetNextItemWidth(width); }
    if (disabled)     { ImGui::BeginDisabled(); }

    bool changed = ImGui::SliderInt(labelBuf, &value, min, max, fmt ? fmt : "%d");

    if (disabled) { ImGui::EndDisabled(); }
    MaybeTooltip(tooltip);
    return changed;
}

bool ShowSliderFloat(std::string_view id, std::string_view label,
                     float& value, float min, float max,
                     std::string_view format, bool disabled,
                     std::string_view tooltip, float width) {
    char labelBuf[256];
    BuildLabelBuf(labelBuf, sizeof(labelBuf), label, id);

    char fmtBuf[64];
    const char* fmt = nullptr;
    if (!format.empty()) {
        MakeNullTerm(fmtBuf, sizeof(fmtBuf), format);
        fmt = fmtBuf;
    }

    if (width > 0.0f) { ImGui::SetNextItemWidth(width); }
    if (disabled)     { ImGui::BeginDisabled(); }

    bool changed = ImGui::SliderFloat(labelBuf, &value, min, max, fmt ? fmt : "%.3f");

    if (disabled) { ImGui::EndDisabled(); }
    MaybeTooltip(tooltip);
    return changed;
}

bool ShowSliderDouble(std::string_view id, std::string_view label,
                      double& value, double min, double max,
                      std::string_view format, bool disabled,
                      std::string_view tooltip, float width) {
    char labelBuf[256];
    BuildLabelBuf(labelBuf, sizeof(labelBuf), label, id);

    char fmtBuf[64];
    const char* fmt = nullptr;
    if (!format.empty()) {
        MakeNullTerm(fmtBuf, sizeof(fmtBuf), format);
        fmt = fmtBuf;
    }

    if (width > 0.0f) { ImGui::SetNextItemWidth(width); }
    if (disabled)     { ImGui::BeginDisabled(); }

    bool changed = ImGui::SliderScalar(labelBuf, ImGuiDataType_Double,
                                        &value, &min, &max,
                                        fmt ? fmt : "%.6f");

    if (disabled) { ImGui::EndDisabled(); }
    MaybeTooltip(tooltip);
    return changed;
}

// ─── Combo helper ─────────────────────────────────────────────────────────────

bool ShowComboImpl(std::string_view id, std::string_view label,
                   int& selectedIdx,
                   std::span<const std::string> labels,
                   bool disabled, std::string_view tooltip, float width) {
    char labelBuf[256];
    BuildLabelBuf(labelBuf, sizeof(labelBuf), label, id);

    const std::string& preview =
        (selectedIdx >= 0 && selectedIdx < static_cast<int>(labels.size()))
            ? labels[static_cast<std::size_t>(selectedIdx)]
            : std::string{};

    if (width > 0.0f) { ImGui::SetNextItemWidth(width); }
    if (disabled)     { ImGui::BeginDisabled(); }

    bool changed = false;
    if (ImGui::BeginCombo(labelBuf, preview.c_str())) {
        for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
            const bool isSelected = (i == selectedIdx);
            if (ImGui::Selectable(labels[static_cast<std::size_t>(i)].c_str(), isSelected)) {
                selectedIdx = i;
                changed     = true;
            }
            if (isSelected) { ImGui::SetItemDefaultFocus(); }
        }
        ImGui::EndCombo();
    }

    if (disabled) { ImGui::EndDisabled(); }
    MaybeTooltip(tooltip);
    return changed;
}

} // namespace ImFrame::Internal
