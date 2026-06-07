/**
 * @file     Radio.hpp
 * @brief    Radio button widget for mutually exclusive option selection
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
 * @class    Radio
 * @brief    Single radio button in a group sharing one `int&` binding
 *
 * Multiple `Radio` instances bound to the same `int&` form a radio group.
 * Each instance represents one option identified by an integer value.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * int mode = 0;
 * Widgets::Radio("Linear",  mode, 0).Show();
 * Widgets::Radio("Nearest", mode, 1).Show();
 * Widgets::Radio("Cubic",   mode, 2).Show();
 * @endcode
 */
class Radio {
public:
    /**
     * @brief    Construct a radio button.
     * @param[in]   label   Text label displayed next to the radio button.
     * @param[in,out]  value   Shared selection state; set to `option` when this button is selected.
     * @param[in]   option  The integer value this radio button represents.
     * @throws   Nothing.
     */
    explicit Radio(std::string_view label, int& value, int option)
        : _label(label), _value(value), _option(option) {}

    Radio& Disabled(bool disabled = true)  { _disabled = disabled;  return *this; }
    Radio& Tooltip(std::string_view tip)   { _tooltip = tip;         return *this; }
    Radio& Width(float w)                  { _width = w;             return *this; }
    Radio& Id(std::string_view id)         { _id = id;               return *this; }

    /**
     * @brief    Render the radio button and update `value` if selected.
     * @return   `true` if this option was selected this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view    _label;
    int&                _value;
    int                 _option;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

} // namespace ImFrame::Widgets
