/**
 * @file     Button.hpp
 * @brief    Push-button widget with optional icon and explicit size
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
 * @class    Button
 * @brief    Immediate-mode push button with fluent builder API
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Button("Save")
 *     .Icon(ImFrame::Icons::Fa::FloppyDisk)
 *     .Size({120.0f, 0.0f})
 *     .OnClick([&] { Save(); })
 *     .Show();
 * @endcode
 */
class Button {
public:
    /**
     * @brief    Construct a button with the given label.
     * @param[in]  label  Text displayed on the button face.
     * @throws   Nothing.
     */
    explicit Button(std::string_view label) : _label(label) {}

    Button& Icon(const char* glyph)                        { _icon = glyph;               return *this; }
    Button& Size(Vec2 size)                                { _size = size;                 return *this; }

    /**
     * @brief    Register a click callback fired when the button is clicked.
     * @param[in]  cb  Zero-argument delegate. Must fit in the 16-byte Delegate buffer.
     * @return   `*this` for chaining.
     */
    Button& OnClick(Utility::Delegate<void()> cb)          { _onClick = std::move(cb);    return *this; }

    Button& Disabled(bool disabled = true)                 { _disabled = disabled;         return *this; }
    Button& Tooltip(std::string_view tip)                  { _tooltip = tip;               return *this; }
    Button& Width(float w)                                 { _width = w;                   return *this; }
    Button& Id(std::string_view id)                        { _id = id;                     return *this; }

    /**
     * @brief    Render the button and fire `OnClick` if clicked.
     * @return   `true` if the button was clicked this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view            _label;
    const char*                 _icon     = nullptr;
    Vec2                        _size     {};
    Utility::Delegate<void()>   _onClick;
    std::string_view            _tooltip;
    std::string_view            _id;
    float                       _width    = 0.0f;
    bool                        _disabled = false;
};

} // namespace ImFrame::Widgets
