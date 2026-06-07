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
