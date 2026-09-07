/**
 * @file     LinePlot.hpp
 * @brief    Multi-series line plot widget wrapping ImPlot::PlotLine
 *
 * Build a `LinePlot` each frame by chaining `Series()` calls, then call
 * `Show()`. Series data is passed as `std::span<const double>` — no copies are
 * made and data ownership remains with the caller for the duration of `Show()`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-10
 * @version  1.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ImFrame::Widgets {

/**
 * @class    LinePlot
 * @brief    Immediate-mode multi-series line chart backed by ImPlot
 *
 * Create inline each frame, chain `Series()` for each data set, then call
 * `Show()`. The internal series list is cleared after every `Show()` call so
 * stored instances accumulate only the series added since the last render.
 *
 * @since    1.5.0
 *
 * @example
 * @code
 * const std::array<double,5> xs{0,1,2,3,4};
 * const std::array<double,5> ys{0,1,4,9,16};
 *
 * LinePlot("##curve")
 *     .Size(400.0f, 300.0f)
 *     .XLabel("x")
 *     .YLabel("y = x²")
 *     .Series("quadratic", xs, ys)
 *     .Show();
 * @endcode
 */
class LinePlot {
public:
    /**
     * @brief    Construct a line plot with the given ImGui/ImPlot identifier.
     * @param[in]  id  Unique plot ID (e.g. `"##myplot"`).
     * @throws   Nothing.
     */
    explicit LinePlot(std::string_view id);

    /**
     * @brief    Set the plot canvas size in pixels.
     * @param[in]  w  Width in pixels. Pass `-1` to fill available width.
     * @param[in]  h  Height in pixels.
     * @return   `*this` for chaining.
     */
    LinePlot& Size(float w, float h);

    /**
     * @brief    Label the X axis.
     * @param[in]  label  Axis label text.
     * @return   `*this` for chaining.
     */
    LinePlot& XLabel(std::string_view label);

    /**
     * @brief    Label the Y axis.
     * @param[in]  label  Axis label text.
     * @return   `*this` for chaining.
     */
    LinePlot& YLabel(std::string_view label);

    /**
     * @brief    Add a named data series.
     *
     * Multiple `Series()` calls accumulate lines. The spans must remain valid
     * until `Show()` returns. Only `min(xs.size(), ys.size())` points are drawn.
     *
     * @param[in]  name  Legend label for this series.
     * @param[in]  xs    X coordinates.
     * @param[in]  ys    Y coordinates.
     * @return   `*this` for chaining.
     */
    LinePlot& Series(std::string_view name,
                     std::span<const double> xs,
                     std::span<const double> ys);

    /**
     * @brief    Render the plot for the current frame.
     *
     * Calls `ImPlot::BeginPlot` / `EndPlot` and renders all accumulated series.
     * Clears the series list after rendering so the next frame starts empty.
     *
     * @throws   Nothing.
     */
    void Show();

private:
    struct SeriesData {
        std::string             name;
        std::span<const double> xs;
        std::span<const double> ys;
    };

    std::string              _id;
    float                    _width  = -1.0f;
    float                    _height = 0.0f;
    std::string              _xLabel;
    std::string              _yLabel;
    std::vector<SeriesData>  _series;
};

} // namespace ImFrame::Widgets
