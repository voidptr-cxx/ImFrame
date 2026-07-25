/**
 * @file     TextRO.cpp
 * @brief    `Text` primitive's concrete `Element` — measurement and draw-list text emission
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Tree/Primitives/Text.hpp"

#include <imgui.h>

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::Widget;
using Tree::Primitives::Text;

class TextElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Text>();
    }

    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Text>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        ImFont*     font     = ImGui::GetFont();
        const float fontSize = _config.GetFontSize() > 0.0f ? _config.GetFontSize() : ImGui::GetFontSize();
        _wrapWidth            = _config.GetWrap() ? constraints.MaxWidth : 0.0f;

        const char* begin = _config.GetContent().data();
        const char* end   = begin + _config.GetContent().size();
        const ImVec2 measured = font->CalcTextSizeA(fontSize, FLT_MAX, _wrapWidth, begin, end);

        _size = constraints.Constrain({measured.x, measured.y});
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        ImFont*     font     = ImGui::GetFont();
        const float fontSize = _config.GetFontSize() > 0.0f ? _config.GetFontSize() : ImGui::GetFontSize();

        const char* begin = _config.GetContent().data();
        const char* end   = begin + _config.GetContent().size();

        float startX = position.x;
        if (_config.GetAlign() != Widgets::TextAlign::Start) {
            const ImVec2 measured = font->CalcTextSizeA(fontSize, FLT_MAX, _wrapWidth, begin, end);
            if (_config.GetAlign() == Widgets::TextAlign::Center) {
                startX = position.x + (_size.x - measured.x) * 0.5f;
            } else { // End
                startX = position.x + (_size.x - measured.x);
            }
        }

        cmd.Push(Rendering::DrawText{
            .Position = {startX, position.y},
            .Text     = _config.GetContent(),
            .FontSize = fontSize,
            .Color    = _config.GetColor(),
            .MaxWidth = _wrapWidth,
        });
    }

private:
    Text  _config{""};
    float _wrapWidth = 0.0f;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree::Primitives {

std::unique_ptr<Element> Text::CreateElement() const {
    return std::make_unique<Internal::TextElement>();
}

} // namespace ImFrame::Tree::Primitives
