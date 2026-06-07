/**
 * @file     Slider.hpp
 * @brief    Numeric range slider templated on arithmetic types
 *
 * `Slider<T>` dispatches to `ImGui::SliderInt` for `int`, `ImGui::SliderFloat`
 * for `float`, and a double-precision scalar path for all other arithmetic types.
 * The ImGui calls are delegated to non-template internal helpers so that
 * `<imgui.h>` is not pulled into this header.
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
#include <type_traits>

// ─── Internal helpers (declared here for template Show() — not part of public API) ─
namespace ImFrame::Internal {

bool ShowSliderInt(std::string_view id, std::string_view label,
                   int& value, int min, int max,
                   std::string_view format, bool disabled,
                   std::string_view tooltip, float width);

bool ShowSliderFloat(std::string_view id, std::string_view label,
                     float& value, float min, float max,
                     std::string_view format, bool disabled,
                     std::string_view tooltip, float width);

bool ShowSliderDouble(std::string_view id, std::string_view label,
                      double& value, double min, double max,
                      std::string_view format, bool disabled,
                      std::string_view tooltip, float width);

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

/**
 * @class    Slider
 * @brief    Immediate-mode slider bound to an arithmetic value with min/max range
 *
 * @tparam   T  Any arithmetic type. `int` and `float` map directly to ImGui sliders;
 *              integral types map to `int`; floating-point types map to `double`.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * float volume = 0.8f;
 * Widgets::Slider<float>("Volume", volume, 0.0f, 1.0f)
 *     .Format("%.2f")
 *     .Width(200.0f)
 *     .Show();
 * @endcode
 */
template<typename T>
    requires std::is_arithmetic_v<T>
class Slider {
public:
    /**
     * @brief    Construct a slider.
     * @param[in]   label  Display label.
     * @param[in,out]  value  Bound value; clamped to `[min, max]` on user interaction.
     * @param[in]   min    Lower bound (inclusive).
     * @param[in]   max    Upper bound (inclusive).
     * @throws   Nothing.
     */
    explicit Slider(std::string_view label, T& value, T min, T max)
        : _label(label), _value(value), _min(min), _max(max) {}

    /**
     * @brief    Override the printf-style display format for the current value.
     * @param[in]  format  Format string (e.g. `"%.2f"`, `"%d%%"`). Empty = ImGui default.
     * @return   `*this` for chaining.
     */
    Slider& Format(std::string_view format) { _format = format;   return *this; }
    Slider& Disabled(bool disabled = true)  { _disabled = disabled; return *this; }
    Slider& Tooltip(std::string_view tip)   { _tooltip = tip;      return *this; }
    Slider& Width(float w)                  { _width = w;          return *this; }
    Slider& Id(std::string_view id)         { _id = id;            return *this; }

    /**
     * @brief    Render the slider and update `value` if dragged.
     * @return   `true` if the value changed this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show() {
        if constexpr (std::is_same_v<T, int>) {
            return Internal::ShowSliderInt(_id, _label, _value, _min, _max,
                                           _format, _disabled, _tooltip, _width);
        } else if constexpr (std::is_same_v<T, float>) {
            return Internal::ShowSliderFloat(_id, _label, _value, _min, _max,
                                             _format, _disabled, _tooltip, _width);
        } else if constexpr (std::is_integral_v<T>) {
            // Promote narrow/wide ints to int for the slider call.
            int v  = static_cast<int>(_value);
            int mn = static_cast<int>(_min);
            int mx = static_cast<int>(_max);
            bool changed = Internal::ShowSliderInt(_id, _label, v, mn, mx,
                                                    _format, _disabled, _tooltip, _width);
            if (changed) { _value = static_cast<T>(v); }
            return changed;
        } else {
            // All remaining floating-point types (double, long double) → double.
            double v  = static_cast<double>(_value);
            double mn = static_cast<double>(_min);
            double mx = static_cast<double>(_max);
            bool changed = Internal::ShowSliderDouble(_id, _label, v, mn, mx,
                                                       _format, _disabled, _tooltip, _width);
            if (changed) { _value = static_cast<T>(v); }
            return changed;
        }
    }

private:
    std::string_view    _label;
    T&                  _value;
    T                   _min;
    T                   _max;
    std::string_view    _format;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

} // namespace ImFrame::Widgets
