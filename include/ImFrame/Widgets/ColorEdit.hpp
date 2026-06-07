/**
 * @file     ColorEdit.hpp
 * @brief    RGBA colour picker widget bound to a Vec4 reference
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

#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    ColorEdit
 * @brief    Immediate-mode RGBA colour editor bound to a `Vec4&` reference
 *
 * `Vec4` fields map to `{r, g, w, b, a}` in normalised `[0, 1]` range.
 * The alpha channel is hidden by default; call `Alpha(true)` to expose it.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Vec4 tint{1.0f, 0.5f, 0.0f, 1.0f};
 * Widgets::ColorEdit("Tint", tint)
 *     .Alpha(true)
 *     .OnChange([&](Vec4 c) { material.SetTint(c); })
 *     .Show();
 * @endcode
 */
class ColorEdit {
public:
    /**
     * @brief    Construct a colour editor bound to a Vec4.
     * @param[in]   label  Display label.
     * @param[in,out]  value  RGBA colour in `[0, 1]` normalised range. Modified in-place.
     * @throws   Nothing.
     */
    explicit ColorEdit(std::string_view label, Vec4& value) : _label(label), _value(value) {}

    /**
     * @brief    Show or hide the alpha channel slider.
     * @param[in]  alpha  `true` = show alpha (default: false).
     * @return   `*this` for chaining.
     */
    ColorEdit& Alpha(bool alpha = true)                        { _alpha = alpha;            return *this; }

    /**
     * @brief    Register a change callback fired when the colour changes.
     * @param[in]  cb  Delegate receiving a copy of the new colour value.
     * @return   `*this` for chaining.
     */
    ColorEdit& OnChange(Utility::Delegate<void(Vec4)> cb)      { _onChange = std::move(cb); return *this; }

    ColorEdit& Disabled(bool disabled = true)                  { _disabled = disabled;      return *this; }
    ColorEdit& Tooltip(std::string_view tip)                   { _tooltip = tip;            return *this; }
    ColorEdit& Width(float w)                                  { _width = w;                return *this; }
    ColorEdit& Id(std::string_view id)                         { _id = id;                  return *this; }

    /**
     * @brief    Render the colour editor and update `value` if changed.
     * @return   `true` if the colour changed this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view                _label;
    Vec4&                           _value;
    Utility::Delegate<void(Vec4)>   _onChange;
    std::string_view                _tooltip;
    std::string_view                _id;
    float                           _width    = 0.0f;
    bool                            _alpha    = false;
    bool                            _disabled = false;
};

} // namespace ImFrame::Widgets
