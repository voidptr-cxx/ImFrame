/**
 * @file     ColorEdit.cpp
 * @brief    Implementation of Widgets::ColorEdit::Show()
 *
 * @internal
 * Converts `Vec4` ↔ `ImVec4` by copying the four float fields. The
 * layout-identity static_assert guards against ABI drift.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/ColorEdit.hpp"
#include "WidgetHelpers.hpp"

#include <imgui.h>

static_assert(sizeof(ImFrame::Widgets::Vec4) == sizeof(ImVec4),
              "Widgets::Vec4 must be layout-identical to ImVec4");

namespace ImFrame::Widgets {

bool ColorEdit::Show() {
    char buf[256];
    Internal::BuildLabelBuf(buf, sizeof(buf), _label, _id);

    ImVec4 color{_value.x, _value.y, _value.z, _value.w};

    if (_width > 0.0f) { ImGui::SetNextItemWidth(_width); }
    if (_disabled)     { ImGui::BeginDisabled(); }

    bool changed = false;
    if (_alpha) {
        changed = ImGui::ColorEdit4(buf, reinterpret_cast<float*>(&color));
    } else {
        changed = ImGui::ColorEdit3(buf, reinterpret_cast<float*>(&color));
    }

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    if (changed) {
        _value = {color.x, color.y, color.z, color.w};
        if (_onChange) { _onChange(_value); }
    }
    return changed;
}

} // namespace ImFrame::Widgets

// ─── ColorEditWidget / ColorEditElement (Phase 30) ──────────────────────────────

namespace ImFrame::Internal {

class ColorEditElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::ColorEditWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::ColorEditWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        const float w = _config.GetWidth() > 0.0f ? _config.GetWidth() : constraints.MaxWidth;
        _size = constraints.Constrain({w, ImGui::GetFrameHeight()});
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);

        Widgets::Vec4* value = _config.GetValue();
        if (value) {
            ImVec4 color{value->x, value->y, value->z, value->w};

            if (_config.GetWidth() > 0.0f) { ImGui::SetNextItemWidth(_config.GetWidth()); }
            if (_config.GetDisabled())     { ImGui::BeginDisabled(); }

            bool changed = _config.GetAlpha()
                ? ImGui::ColorEdit4(_config.GetLabel().c_str(), reinterpret_cast<float*>(&color))
                : ImGui::ColorEdit3(_config.GetLabel().c_str(), reinterpret_cast<float*>(&color));

            if (_config.GetDisabled()) { ImGui::EndDisabled(); }

            if (!_config.GetTooltip().empty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
            }

            if (changed) {
                *value = {color.x, color.y, color.z, color.w};
                if (_config.GetOnChange()) { _config.GetOnChange()(*value); }
            }
        }

        ImGui::PopID();
    }

private:
    Widgets::ColorEditWidget _config{"", nullptr};
};

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

std::unique_ptr<Tree::Element> ColorEditWidget::CreateElement() const {
    return std::make_unique<Internal::ColorEditElement>();
}

} // namespace ImFrame::Widgets
