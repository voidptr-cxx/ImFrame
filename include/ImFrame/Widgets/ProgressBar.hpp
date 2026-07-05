/**
 * @file     ProgressBar.hpp
 * @brief    Determinate progress bar widget
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
 * @class    ProgressBar
 * @brief    Horizontal progress bar displaying a `[0, 1]` fraction
 *
 * `Show()` always returns `false` — progress bars are not interactive.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::ProgressBar(loadProgress)
 *     .Size({-1.0f, 0.0f})
 *     .Overlay("Loading assets...")
 *     .Show();
 * @endcode
 */
/// @deprecated Phase 10–14 imperative widget API, not yet reimplemented as a Tree Component. See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.
class [[deprecated("See Docs/Migration_v1_to_v2.md.")]] ProgressBar {
public:
    /**
     * @brief    Construct a progress bar.
     * @param[in]  fraction  Completion ratio in `[0, 1]`. Values outside this range are clamped by ImGui.
     * @throws   Nothing.
     */
    explicit ProgressBar(float fraction) : _fraction(fraction) {}

    /**
     * @brief    Set explicit display size.
     * @param[in]  size  Width × height in pixels. `-1` in either axis fills the available space.
     * @return   `*this` for chaining.
     */
    ProgressBar& Size(Vec2 size)            { _size = size;      return *this; }

    /**
     * @brief    Draw overlay text centred on the bar.
     * @param[in]  text  Text shown on top of the progress bar.
     * @return   `*this` for chaining.
     */
    ProgressBar& Overlay(std::string_view text) { _overlay = text; return *this; }

    ProgressBar& Disabled(bool d = true)    { _disabled = d;     return *this; }
    ProgressBar& Tooltip(std::string_view tip) { _tooltip = tip; return *this; }
    ProgressBar& Width(float w)             { _width = w;        return *this; }
    ProgressBar& Id(std::string_view id)    { _id = id;          return *this; }

    /**
     * @brief    Render the progress bar. Always returns `false`.
     * @return   `false` — progress bars are not interactive.
     * @throws   Nothing.
     */
    bool Show();

private:
    float               _fraction;
    Vec2                _size     {-1.0f, 0.0f};
    std::string_view    _overlay;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

} // namespace ImFrame::Widgets
