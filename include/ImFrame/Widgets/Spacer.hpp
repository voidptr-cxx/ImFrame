/**
 * @file     Spacer.hpp
 * @brief    Fixed-size invisible spacing widget using ImGui::Dummy()
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
 * @class    Spacer
 * @brief    Inserts a fixed-size blank area in the layout via `ImGui::Dummy()`
 *
 * `Show()` always returns `false` — spacers are not interactive.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Spacer({0.0f, 8.0f}).Show();  // 8 px vertical gap
 * Widgets::Spacer({12.0f, 0.0f}).Show(); // 12 px horizontal gap
 * @endcode
 */
/// @deprecated Use `Tree::Primitives::SizedBox` (fixed gap) or `Tree::Primitives::Spacer` (flexible gap) instead. See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.
class [[deprecated("See Docs/Migration_v1_to_v2.md.")]] Spacer {
public:
    /**
     * @brief    Construct a spacer with a given pixel size.
     * @param[in]  size  Width × height in pixels. Zero component occupies no space on that axis.
     * @throws   Nothing.
     */
    explicit Spacer(Vec2 size = Vec2{}) : _size(size) {}

    Spacer& Disabled(bool d = true)  { _disabled = d;   return *this; }
    Spacer& Tooltip(std::string_view tip) { _tooltip = tip; return *this; }
    Spacer& Width(float w)           { _width = w;       return *this; }
    Spacer& Id(std::string_view id)  { _id = id;         return *this; }

    /**
     * @brief    Render the spacer. Always returns `false`.
     * @return   `false` — spacers are not interactive.
     * @throws   Nothing.
     */
    bool Show();

private:
    Vec2                _size;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

} // namespace ImFrame::Widgets
