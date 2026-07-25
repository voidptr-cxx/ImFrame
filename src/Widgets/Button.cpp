/**
 * @file     Button.cpp
 * @brief    Implementation of ButtonWidget's Element (Mount/Update/Layout/Paint)
 *
 * @internal
 * ImGui headers are confined to this translation unit. The public header
 * (Button.hpp) contains no ImGui includes, preserving the architecture invariant.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/Button.hpp"

#include <imgui.h>

static_assert(sizeof(ImFrame::Widgets::Vec2) == sizeof(ImVec2),
              "Widgets::Vec2 must be layout-identical to ImVec2");

// ─── ButtonWidget / ButtonElement (Phase 29) ────────────────────────────────────

namespace ImFrame::Internal {

class ButtonElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::ButtonWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::ButtonWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        const std::string  displayLabel = BuildDisplayLabel();
        const ImVec2       labelSize    = ImGui::CalcTextSize(displayLabel.c_str());
        const ImGuiStyle&  style        = ImGui::GetStyle();
        const Widgets::Vec2 explicitSize = _config.GetSize();

        float w = explicitSize.x > 0.0f ? explicitSize.x : labelSize.x + style.FramePadding.x * 2.0f;
        float h = explicitSize.y > 0.0f ? explicitSize.y : labelSize.y + style.FramePadding.y * 2.0f;
        if (_config.GetWidth() > 0.0f) { w = _config.GetWidth(); }

        _size = constraints.Constrain({w, h});
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 position) override {
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);

        const std::string displayLabel = BuildDisplayLabel();

        if (_config.GetDisabled()) { ImGui::BeginDisabled(); }
        // Pass the same size committed to during Layout() so the actual ImGui::Button()
        // call never silently disagrees with what the tree already laid out around it.
        const bool clicked = ImGui::Button(displayLabel.c_str(), ImVec2{_size.x, _size.y});
        if (_config.GetDisabled()) { ImGui::EndDisabled(); }

        if (!_config.GetTooltip().empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
        }

        if (clicked && _config.GetOnClick()) { _config.GetOnClick()(); }

        ImGui::PopID();
    }

private:
    [[nodiscard]] std::string BuildDisplayLabel() const {
        if (!_config.GetIcon().empty()) {
            // NBSP NBSP separator between glyph and label text.
            return _config.GetIcon() + "\xc2\xa0\xc2\xa0" + _config.GetLabel();
        }
        return _config.GetLabel();
    }

    Widgets::ButtonWidget _config{""};
};

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

std::unique_ptr<Tree::Element> ButtonWidget::CreateElement() const {
    return std::make_unique<Internal::ButtonElement>();
}

} // namespace ImFrame::Widgets
