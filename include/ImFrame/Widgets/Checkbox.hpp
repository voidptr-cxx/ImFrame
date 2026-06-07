/**
 * @file     Checkbox.hpp
 * @brief    Boolean toggle checkbox widget
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
 * @class    Checkbox
 * @brief    Immediate-mode checkbox bound to a `bool` reference
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * bool wireframe = false;
 * Widgets::Checkbox("Wireframe", wireframe)
 *     .OnChange([&](bool v) { renderer.SetWireframe(v); })
 *     .Show();
 * @endcode
 */
class Checkbox {
public:
    /**
     * @brief    Construct a checkbox bound to a boolean value.
     * @param[in]   label  Text label displayed next to the checkbox.
     * @param[in,out]  value  Boolean state toggled by the widget.
     * @throws   Nothing.
     */
    explicit Checkbox(std::string_view label, bool& value) : _label(label), _value(value) {}

    /**
     * @brief    Register a change callback fired when the checkbox is toggled.
     * @param[in]  cb  Delegate receiving the new boolean state.
     * @return   `*this` for chaining.
     */
    Checkbox& OnChange(Utility::Delegate<void(bool)> cb)   { _onChange = std::move(cb);   return *this; }
    Checkbox& Disabled(bool disabled = true)               { _disabled = disabled;         return *this; }
    Checkbox& Tooltip(std::string_view tip)                { _tooltip = tip;               return *this; }
    Checkbox& Width(float w)                               { _width = w;                   return *this; }
    Checkbox& Id(std::string_view id)                      { _id = id;                     return *this; }

    /**
     * @brief    Render the checkbox and update `value` if toggled.
     * @return   `true` if the state changed this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view                _label;
    bool&                           _value;
    Utility::Delegate<void(bool)>   _onChange;
    std::string_view                _tooltip;
    std::string_view                _id;
    float                           _width    = 0.0f;
    bool                            _disabled = false;
};

} // namespace ImFrame::Widgets
