/**
 * @file     Radio.cpp
 * @brief    Implementation of RadioWidget's Element
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

#include "ImFrame/Widgets/Radio.hpp"

#include <imgui.h>

// ─── RadioWidget / RadioElement (Phase 30) ──────────────────────────────────────

namespace ImFrame::Internal {

class RadioElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::RadioWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::RadioWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        const ImVec2 labelSize = ImGui::CalcTextSize(_config.GetLabel().c_str());
        const ImGuiStyle& style = ImGui::GetStyle();
        const float radioDiameter = ImGui::GetFrameHeight();
        float w = radioDiameter + style.ItemInnerSpacing.x + labelSize.x;
        if (_config.GetWidth() > 0.0f) { w = _config.GetWidth(); }
        _size = constraints.Constrain({w, labelSize.y});
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 position) override {
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);

        int* value = _config.GetValue();
        if (value) {
            if (_config.GetWidth() > 0.0f) { ImGui::SetNextItemWidth(_config.GetWidth()); }
            if (_config.GetDisabled())     { ImGui::BeginDisabled(); }

            ImGui::RadioButton(_config.GetLabel().c_str(), value, _config.GetOption());

            if (_config.GetDisabled()) { ImGui::EndDisabled(); }

            if (!_config.GetTooltip().empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
            }
        }

        ImGui::PopID();
    }

private:
    Widgets::RadioWidget _config{"", nullptr, 0};
};

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

std::unique_ptr<Tree::Element> RadioWidget::CreateElement() const {
    return std::make_unique<Internal::RadioElement>();
}

} // namespace ImFrame::Widgets
