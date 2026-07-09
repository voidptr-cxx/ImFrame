/**
 * @file     Checkbox.cpp
 * @brief    Implementation of CheckboxWidget's Element
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

#include <imgui.h>

#include <algorithm>

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
