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

#include <algorithm>

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

// ─── CheckboxWidget / CheckboxElement (Phase 29) ────────────────────────────────

namespace ImFrame::Internal {

class CheckboxElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::CheckboxWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::CheckboxWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        const float       frameHeight = ImGui::GetFrameHeight();
        const ImVec2      boxSize{frameHeight, frameHeight};
        const ImVec2      labelSize = ImGui::CalcTextSize(_config.GetLabel().c_str());
        const ImGuiStyle& style     = ImGui::GetStyle();
        const float w = boxSize.x + (labelSize.x > 0.0f ? style.ItemInnerSpacing.x + labelSize.x : 0.0f);
        const float h = std::max(boxSize.y, labelSize.y);
        _size = constraints.Constrain({w, h});
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);

        if (_config.GetWidth() > 0.0f) { ImGui::SetNextItemWidth(_config.GetWidth()); }
        if (_config.GetDisabled()) { ImGui::BeginDisabled(); }

        bool* value   = _config.GetValue();
        bool  changed = value && ImGui::Checkbox(_config.GetLabel().c_str(), value);

        if (_config.GetDisabled()) { ImGui::EndDisabled(); }

        if (!_config.GetTooltip().empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
        }

        if (changed && _config.GetOnChange()) { _config.GetOnChange()(*value); }

        ImGui::PopID();
    }

private:
    Widgets::CheckboxWidget _config{"", nullptr};
};

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

std::unique_ptr<Tree::Element> CheckboxWidget::CreateElement() const {
    return std::make_unique<Internal::CheckboxElement>();
}

} // namespace ImFrame::Widgets
