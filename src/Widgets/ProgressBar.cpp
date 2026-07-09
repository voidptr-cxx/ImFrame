/**
 * @file     ProgressBar.cpp
 * @brief    Implementation of ProgressBarWidget's Element
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

#include <imgui.h>

// ─── ProgressBarWidget / ProgressBarElement (Phase 30) ──────────────────────────

namespace ImFrame::Internal {

class ProgressBarElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::ProgressBarWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::ProgressBarWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        const Widgets::Vec2 explicitSize = _config.GetSize();
        const float w = explicitSize.x > 0.0f ? explicitSize.x : constraints.MaxWidth;
        const float h = explicitSize.y > 0.0f ? explicitSize.y : ImGui::GetFrameHeight();
        _size = constraints.Constrain({w, h});
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);

        if (_config.GetWidth() > 0.0f) { ImGui::SetNextItemWidth(_config.GetWidth()); }
        if (_config.GetDisabled())     { ImGui::BeginDisabled(); }

        const char* overlayPtr = _config.GetOverlay().empty() ? nullptr : _config.GetOverlay().c_str();
        ImGui::ProgressBar(_config.GetFraction(), ImVec2{_size.x, _size.y}, overlayPtr);

        if (_config.GetDisabled()) { ImGui::EndDisabled(); }

        if (!_config.GetTooltip().empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
        }

        ImGui::PopID();
    }

private:
    Widgets::ProgressBarWidget _config{0.0f};
};

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

std::unique_ptr<Tree::Element> ProgressBarWidget::CreateElement() const {
    return std::make_unique<Internal::ProgressBarElement>();
}

} // namespace ImFrame::Widgets
