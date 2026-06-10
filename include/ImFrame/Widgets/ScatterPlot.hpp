/**
 * @file     ScatterPlot.hpp
 * @brief    Multi-series scatter plot widget wrapping ImPlot::PlotScatter
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
#include <vector>

namespace ImFrame::Widgets {

/**
 * @class    ScatterPlot
 * @brief    Immediate-mode multi-series scatter chart backed by ImPlot
 *
 * @since    1.5.0
 *
 * @example
 * @code
 * const std::array<double,4> xs{1.0, 2.0, 3.0, 4.0};
 * const std::array<double,4> ys{1.1, 3.9, 9.1, 16.2};
 *
 * ScatterPlot("##scatter")
 *     .Size(400.0f, 300.0f)
 *     .MarkerSize(6.0f)
 *     .Points("measurements", xs, ys)
 *     .Show();
 * @endcode
 */
class ScatterPlot {
public:
    /**
     * @brief    Construct a scatter plot with the given identifier.
     * @param[in]  id  Unique plot ID.
     * @throws   Nothing.
     */
    explicit ScatterPlot(std::string_view id);

    /**
     * @brief    Set the plot canvas size in pixels.
     * @param[in]  w  Width. Pass `-1` to fill available width.
     * @param[in]  h  Height.
     * @return   `*this` for chaining.
     */
    ScatterPlot& Size(float w, float h);

    /**
     * @brief    Add a named point series.
     *
     * Spans must remain valid until `Show()` returns.
     *
     * @param[in]  name  Legend label.
     * @param[in]  xs    X coordinates.
     * @param[in]  ys    Y coordinates.
     * @return   `*this` for chaining.
     */
    ScatterPlot& Points(std::string_view name,
                        std::span<const double> xs,
                        std::span<const double> ys);

    /**
     * @brief    Set the marker radius in pixels.
     * @param[in]  size  Marker radius (default 4.0).
     * @return   `*this` for chaining.
     */
    ScatterPlot& MarkerSize(float size);

    /**
     * @brief    Render the scatter plot for the current frame.
     *
     * Clears accumulated point series after rendering.
     *
     * @throws   Nothing.
     */
    void Show();

private:
    struct PointsData {
        std::string             name;
        std::span<const double> xs;
        std::span<const double> ys;
    };

    std::string              _id;
    float                    _width      = -1.0f;
    float                    _height     = 0.0f;
    float                    _markerSize = 4.0f;
    std::vector<PointsData>  _points;
};

} // namespace ImFrame::Widgets
