/**
 * @file     Text.hpp
 * @brief    Static text display widget with colour, wrap, and disabled variants
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
 * @class    Text
 * @brief    Non-interactive text label with optional colour, wrapping, and dimming
 *
 * Only one of `Colored`, `Wrapped`, or `Disabled` should be set per instance;
 * the priority order is: Disabled → Colored → Wrapped → plain.
 *
 * `Show()` always returns `false` — text labels are not interactive.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Text("Hello, ImFrame!").Show();
 * Widgets::Text("Warning: ").Colored({1.0f, 0.5f, 0.0f, 1.0f}).Show();
 * Widgets::Text(longDescription).Wrapped().Show();
 * @endcode
 */
class Text {
public:
    /**
     * @brief    Construct a text widget.
     * @param[in]  text  String to display. The view must remain valid until `Show()` returns.
     * @throws   Nothing.
     */
    explicit Text(std::string_view text) : _text(text) {}

    /**
     * @brief    Render the text using `ImGui::TextColored()`.
     * @param[in]  color  RGBA text colour in `[0, 1]` normalised range.
     * @return   `*this` for chaining.
     */
    Text& Colored(Vec4 color) { _color = color; _colored = true; return *this; }

    /**
     * @brief    Wrap the text at the current window edge via `ImGui::TextWrapped()`.
     * @param[in]  wrapped  `true` (default) enables word-wrap.
     * @return   `*this` for chaining.
     */
    Text& Wrapped(bool wrapped = true) { _wrapped = wrapped; return *this; }

    /**
     * @brief    Dim the text via `ImGui::TextDisabled()` (greyed-out appearance).
     * @param[in]  disabled  `true` (default) applies the disabled style.
     * @return   `*this` for chaining.
     */
    Text& Disabled(bool disabled = true) { _disabled = disabled; return *this; }

    Text& Tooltip(std::string_view tip) { _tooltip = tip; return *this; }
    Text& Width(float w)                { _width = w;     return *this; }
    Text& Id(std::string_view id)       { _id = id;       return *this; }

    /**
     * @brief    Render the text label. Always returns `false`.
     * @return   `false` — text labels are not interactive.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view    _text;
    Vec4                _color    {};
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _colored  = false;
    bool                _wrapped  = false;
    bool                _disabled = false;
};

} // namespace ImFrame::Widgets
