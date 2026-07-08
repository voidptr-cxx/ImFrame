/**
 * @file     Image.cpp
 * @brief    Implementation of Widgets::Image::Show() and ShowButton()
 *
 * @internal
 * ImGui 1.92+ uses ImTextureRef (wrapping ImTextureID = ImU64) for all texture
 * parameters. TextureHandle (void*) is converted via intptr_t to preserve the
 * pointer value on both 32-bit and 64-bit targets.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/Image.hpp"
#include "WidgetHelpers.hpp"

#include <imgui.h>

#include <cstdio>
#include <cstdint>

namespace ImFrame::Widgets {

bool Image::Show() {
    if (_disabled) { ImGui::BeginDisabled(); }

    const ImTextureRef texRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(_texture)));
    const ImVec2 sz    {_size.x,    _size.y};
    const ImVec2 uv0   {_uv0.x,    _uv0.y};
    const ImVec2 uv1   {_uv1.x,    _uv1.y};
    const ImVec4 tint  {_tint.x,   _tint.y,   _tint.z,   _tint.w};
    const ImVec4 border{_border.x, _border.y, _border.z, _border.w};

    ImGui::Image(texRef, sz, uv0, uv1, tint, border);

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    return false;
}

bool Image::ShowButton() {
    char idBuf[128];
    if (_id.empty()) {
        std::snprintf(idBuf, sizeof(idBuf), "##img_%p", _texture);
    } else {
        Internal::BuildLabelBuf(idBuf, sizeof(idBuf), _id, {});
    }

    if (_disabled) { ImGui::BeginDisabled(); }

    const ImTextureRef texRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(_texture)));
    const ImVec2 sz  {_size.x,  _size.y};
    const ImVec2 uv0 {_uv0.x,  _uv0.y};
    const ImVec2 uv1 {_uv1.x,  _uv1.y};
    const ImVec4 bg  {0.0f,    0.0f,    0.0f,    0.0f};
    const ImVec4 tint{_tint.x, _tint.y, _tint.z, _tint.w};

    bool clicked = ImGui::ImageButton(idBuf, texRef, sz, uv0, uv1, bg, tint);

    if (_disabled) { ImGui::EndDisabled(); }

    if (!_tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%.*s",
                          static_cast<int>(_tooltip.size()), _tooltip.data());
    }

    return clicked;
}

} // namespace ImFrame::Widgets

// ─── ImageWidget / ImageElement (Phase 30) ──────────────────────────────────────

namespace ImFrame::Internal {

class ImageElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::ImageWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::ImageWidget>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        _size = constraints.Constrain(_config.GetSize());
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);

        const auto  texture = _config.GetTexture();
        const ImTextureRef texRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture)));
        const Widgets::Vec2 sizeF  = _size;
        const Widgets::Vec2 uv0F   = _config.GetUV0();
        const Widgets::Vec2 uv1F   = _config.GetUV1();
        const Widgets::Vec4 tintF  = _config.GetTint();
        const ImVec2 sz  {sizeF.x, sizeF.y};
        const ImVec2 uv0 {uv0F.x,  uv0F.y};
        const ImVec2 uv1 {uv1F.x,  uv1F.y};
        const ImVec4 tint{tintF.x, tintF.y, tintF.z, tintF.w};

        if (_config.GetDisabled()) { ImGui::BeginDisabled(); }

        bool clicked = false;
        if (_config.GetOnClick()) {
            const ImVec4 bg{0.0f, 0.0f, 0.0f, 0.0f};
            clicked = ImGui::ImageButton("##img", texRef, sz, uv0, uv1, bg, tint);
        } else {
            const Widgets::Vec4 borderF = _config.GetBorderColor();
            const ImVec4 border{borderF.x, borderF.y, borderF.z, borderF.w};
            ImGui::Image(texRef, sz, uv0, uv1, tint, border);
        }

        if (_config.GetDisabled()) { ImGui::EndDisabled(); }

        if (!_config.GetTooltip().empty() && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", _config.GetTooltip().c_str());
        }

        if (clicked && _config.GetOnClick()) { _config.GetOnClick()(); }

        ImGui::PopID();
    }

private:
    Widgets::ImageWidget _config{nullptr, Widgets::Vec2{}};
};

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

std::unique_ptr<Tree::Element> ImageWidget::CreateElement() const {
    return std::make_unique<Internal::ImageElement>();
}

} // namespace ImFrame::Widgets
