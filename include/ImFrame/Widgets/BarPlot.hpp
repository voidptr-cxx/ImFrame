/**
 * @file     BarPlot.hpp
 * @brief    Vertical or horizontal bar chart widget wrapping ImPlot::PlotBars
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-10
 * @version  1.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <span>
#include <string>
#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    BarPlot
 * @brief    Single-series bar chart — vertical by default, optionally horizontal
 *
 * @since    1.5.0
 *
 * @example
 * @code
 * const std::array<double,4> vals{12.0, 7.5, 9.0, 14.2};
 *
 * BarPlot("##bars")
 *     .Size(400.0f, 300.0f)
 *     .Bars("Revenue", vals, 0.6)
 *     .Show();
 * @endcode
 */
class BarPlot {
public:
    /**
     * @brief    Construct a bar plot with the given identifier.
     * @param[in]  id  Unique plot ID.
     * @throws   Nothing.
     */
    explicit BarPlot(std::string_view id);

    /**
     * @brief    Set the plot canvas size in pixels.
     * @param[in]  w  Width. Pass `-1` to fill available width.
     * @param[in]  h  Height.
     * @return   `*this` for chaining.
     */
    BarPlot& Size(float w, float h);

    /**
     * @brief    Configure the bar series.
     *
     * @param[in]  name      Legend label.
     * @param[in]  values    Bar heights (or lengths for horizontal).
     * @param[in]  barWidth  Fractional bar width in category units (default 0.67).
     * @return   `*this` for chaining.
     */
    BarPlot& Bars(std::string_view name,
                  std::span<const double> values,
                  double barWidth = 0.67);

    /**
     * @brief    Switch to horizontal orientation.
     * @param[in]  horizontal  `true` draws bars left-to-right (default `true`).
     * @return   `*this` for chaining.
     */
    BarPlot& Horizontal(bool horizontal = true);

    /**
     * @brief    Render the bar chart for the current frame.
     * @throws   Nothing.
     */
    void Show();

private:
    std::string             _id;
    float                   _width      = -1.0f;
    float                   _height     = 0.0f;
    std::string             _seriesName;
    std::span<const double> _values;
    double                  _barWidth   = 0.67;
    bool                    _horizontal = false;
};

} // namespace ImFrame::Widgets
