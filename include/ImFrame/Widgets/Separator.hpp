/**
 * @file     Separator.hpp
 * @brief    Horizontal visual separator line, optionally with a centred label
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
 * @class    Separator
 * @brief    Horizontal line separator; labelled via `ImGui::SeparatorText()` when text is set
 *
 * `Show()` always returns `false` — separators are not interactive.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Separator().Show();                    // plain line
 * Widgets::Separator().Label("Advanced").Show();  // labelled section divider
 * @endcode
 */
/// @deprecated Use a `Tree::Primitives::Box` with a thin `Height`/`Width` and `Background` instead. See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.
class [[deprecated("See Docs/Migration_v1_to_v2.md.")]] Separator {
public:
    Separator() = default;

    /**
     * @brief    Display a label centred in the separator via `ImGui::SeparatorText()`.
     * @param[in]  text  Label text. Empty string = plain separator line.
     * @return   `*this` for chaining.
     */
    Separator& Label(std::string_view text) { _label = text; return *this; }

    Separator& Disabled(bool d = true)  { _disabled = d;   return *this; }
    Separator& Tooltip(std::string_view tip) { _tooltip = tip; return *this; }
    Separator& Width(float w)           { _width = w;       return *this; }
    Separator& Id(std::string_view id)  { _id = id;         return *this; }

    /**
     * @brief    Render the separator. Always returns `false`.
     * @return   `false` — separators are not interactive.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view    _label;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

} // namespace ImFrame::Widgets
