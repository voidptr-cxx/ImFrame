/**
 * @file     HeatMap.hpp
 * @brief    2D heat-map widget wrapping ImPlot::PlotHeatmap
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-10
 * @version  1.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Widgets/PlotContext.hpp"

#include <span>
#include <string>
#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    HeatMap
 * @brief    Colour-mapped 2D grid visualisation backed by ImPlot
 *
 * @since    1.5.0
 *
 * @example
 * @code
 * const std::array<double,9> data{1,2,3, 4,5,6, 7,8,9};
 *
 * HeatMap("##hm")
 *     .Size(400.0f, 300.0f)
 *     .Data(data, 3, 3)
 *     .ColorMap(PlotColormap::Viridis)
 *     .ScaleMin(1.0)
 *     .ScaleMax(9.0)
 *     .Show();
 * @endcode
 */
class HeatMap {
public:
    /**
     * @brief    Construct a heat-map with the given identifier.
     * @param[in]  id  Unique plot ID.
     * @throws   Nothing.
     */
    explicit HeatMap(std::string_view id);

    /**
     * @brief    Set the plot canvas size in pixels.
     * @param[in]  w  Width. Pass `-1` to fill available width.
     * @param[in]  h  Height.
     * @return   `*this` for chaining.
     */
    HeatMap& Size(float w, float h);

    /**
     * @brief    Supply the grid data.
     *
     * Data is stored in row-major order: `data[r * cols + c]`.
     * The span must remain valid until `Show()` returns.
     *
     * @param[in]  data  Flat row-major grid values.
     * @param[in]  rows  Number of rows.
     * @param[in]  cols  Number of columns.
     * @return   `*this` for chaining.
     */
    HeatMap& Data(std::span<const double> data, int rows, int cols);

    /**
     * @brief    Select the colour map.
     * @param[in]  colormap  Built-in ImPlot colormap (default: `PlotColormap::Viridis`).
     * @return   `*this` for chaining.
     */
    HeatMap& ColorMap(PlotColormap colormap);

    /**
     * @brief    Set the value that maps to the lowest colormap colour.
     * @param[in]  min  Scale minimum (default 0.0).
     * @return   `*this` for chaining.
     */
    HeatMap& ScaleMin(double min);

    /**
     * @brief    Set the value that maps to the highest colormap colour.
     * @param[in]  max  Scale maximum (default 1.0).
     * @return   `*this` for chaining.
     */
    HeatMap& ScaleMax(double max);

    /**
     * @brief    Render the heat-map for the current frame.
     * @throws   Nothing.
     */
    void Show();

private:
    std::string             _id;
    float                   _width    = -1.0f;
    float                   _height   = 0.0f;
    std::span<const double> _data;
    int                     _rows     = 0;
    int                     _cols     = 0;
    PlotColormap            _colormap = PlotColormap::Viridis;
    double                  _scaleMin = 0.0;
    double                  _scaleMax = 1.0;
};

} // namespace ImFrame::Widgets
