/**
 * @file     Image.hpp
 * @brief    GPU-texture image display and image-button widget
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Widgets/Types.hpp"

#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    Image
 * @brief    Displays a GPU texture via `ImGui::Image()`; optionally as a clickable button
 *
 * UV coordinates default to the full texture `(0,0)→(1,1)`. Tint defaults to
 * white (no tint). Border colour defaults to transparent (no border).
 *
 * `Show()` always returns `false` — images are not interactive.
 * `ShowButton()` wraps with `ImGui::ImageButton()` and returns `true` on click.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Image(myTex, {256.0f, 256.0f})
 *     .Tint({1.0f, 1.0f, 1.0f, 0.8f})
 *     .Show();
 *
 * if (Widgets::Image(iconTex, {32.0f, 32.0f}).ShowButton()) { DoAction(); }
 * @endcode
 */
class Image {
public:
    /**
     * @brief    Construct an image widget.
     * @param[in]  texture  GPU texture handle (ImTextureID).
     * @param[in]  size     Display size in pixels.
     * @throws   Nothing.
     */
    explicit Image(TextureHandle texture, Vec2 size)
        : _texture(texture), _size(size) {}

    Image& UV0(Vec2 uv)          { _uv0 = uv;          return *this; }
    Image& UV1(Vec2 uv)          { _uv1 = uv;          return *this; }
    Image& Tint(Vec4 tint)       { _tint = tint;        return *this; }
    Image& BorderColor(Vec4 col) { _border = col;       return *this; }
    Image& Disabled(bool d = true) { _disabled = d;    return *this; }
    Image& Tooltip(std::string_view tip) { _tooltip = tip; return *this; }
    Image& Width(float w)        { _width = w;          return *this; }
    Image& Id(std::string_view id) { _id = id;          return *this; }

    /**
     * @brief    Render the image. Always returns `false`.
     * @return   `false` — images are not interactive.
     * @throws   Nothing.
     */
    bool Show();

    /**
     * @brief    Render the image as a clickable button.
     * @return   `true` if clicked this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool ShowButton();

private:
    TextureHandle       _texture;
    Vec2                _size;
    Vec2                _uv0      {0.0f, 0.0f};
    Vec2                _uv1      {1.0f, 1.0f};
    Vec4                _tint     {1.0f, 1.0f, 1.0f, 1.0f};
    Vec4                _border   {0.0f, 0.0f, 0.0f, 0.0f};
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

} // namespace ImFrame::Widgets
