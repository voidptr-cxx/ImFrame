/**
 * @file     Separator.cpp
 * @brief    Implementation of SeparatorWidget's Element
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

#include "ImFrame/Widgets/Separator.hpp"

#include <imgui.h>

// ─── SeparatorWidget / SeparatorElement (Phase 30) ──────────────────────────────

namespace ImFrame::Internal {

class SeparatorElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::SeparatorWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::SeparatorWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        const float height = _config.GetLabel().empty()
            ? ImGui::GetStyle().ItemSpacing.y + 1.0f
            : ImGui::GetTextLineHeight() + ImGui::GetStyle().ItemSpacing.y * 2.0f;
        _size = constraints.Constrain({constraints.MaxWidth, height});
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);

        if (_config.GetDisabled()) { ImGui::BeginDisabled(); }

        if (_config.GetLabel().empty()) {
            ImGui::Separator();
        } else {
            ImGui::SeparatorText(_config.GetLabel().c_str());
        }

        if (_config.GetDisabled()) { ImGui::EndDisabled(); }

        if (!_config.GetTooltip().empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
        }

        ImGui::PopID();
    }

private:
    Widgets::SeparatorWidget _config;
};

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

std::unique_ptr<Tree::Element> SeparatorWidget::CreateElement() const {
    return std::make_unique<Internal::SeparatorElement>();
}

} // namespace ImFrame::Widgets
