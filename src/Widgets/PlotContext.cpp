/**
 * @file     PlotContext.cpp
 * @brief    ImPlot context lifecycle, theme integration, and plot widget Show() implementations
 *
 * @internal
 * All ImPlot API calls are confined to this single translation unit so that
 * <implot.h> never leaks into public headers. The four plot widget classes
 * (LinePlot, BarPlot, ScatterPlot, HeatMap) declare their Show() methods as
 * non-inline; they are defined here alongside PlotContext.
 *
 * PlotScope is a file-local RAII guard that calls ImPlot::EndPlot() only when
 * BeginPlot() returned true — matching ImPlot's own contract.
 *
 * implot v1.0 API notes:
 * - ImPlotCol_Line was removed; per-item line color lives in ImPlotSpec::LineColor.
 * - ImPlotStyleVar_MarkerSize was removed; use ImPlotSpec::MarkerSize instead.
 * - PlotBars flags are passed via ImPlotSpec(ImPlotProp_Flags, ...).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-10
 * @version  1.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Widgets/PlotContext.hpp"
#include "ImFrame/Widgets/LinePlot.hpp"
#include "ImFrame/Widgets/BarPlot.hpp"
#include "ImFrame/Widgets/ScatterPlot.hpp"
#include "ImFrame/Widgets/HeatMap.hpp"
#include "ImFrame/Theme/Theme.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>

namespace {

// File-local RAII scope — mirrors ChildScope/PopupScope conventions.
struct PlotScope {
    explicit PlotScope(bool open) noexcept : _open(open) {}
    ~PlotScope() { if (_open) ImPlot::EndPlot(); }
    PlotScope(const PlotScope&)            = delete;
    PlotScope& operator=(const PlotScope&) = delete;
    PlotScope(PlotScope&& o) noexcept : _open(o._open) { o._open = false; }
    PlotScope& operator=(PlotScope&&)      = delete;
    explicit operator bool() const noexcept { return _open; }
    bool _open;
};

// Convert ColorToken to ImVec4 for ImPlot style assignment.
inline ImVec4 ToImVec4(const ImFrame::Theme::ColorToken& c) noexcept {
    return ImVec4(c.r, c.g, c.b, c.a);
}

} // namespace

namespace ImFrame::Widgets {

// ─── PlotContext ──────────────────────────────────────────────────────────────

void PlotContext::Init() {
    _ctx = ImPlot::CreateContext();
}

void PlotContext::Shutdown() {
    if (_ctx) {
        ImPlot::DestroyContext(_ctx);
        _ctx = nullptr;
    }
}

void PlotContext::ApplyTheme(const ImFrame::Theme::Theme& theme) {
    ImPlotStyle& style = ImPlot::GetStyle();
    // ImPlotCol_Line was removed in implot v1.0; per-item line color is set via ImPlotSpec.
    // Map the remaining per-plot style colors to ImFrame theme tokens.
    style.Colors[ImPlotCol_PlotBg]  = ToImVec4(theme.backgroundSecondary);
    style.Colors[ImPlotCol_FrameBg] = ToImVec4(theme.surfaceDefault);
    style.Colors[ImPlotCol_AxisText]= ToImVec4(theme.textSecondary);
}

// ─── LinePlot ─────────────────────────────────────────────────────────────────

LinePlot::LinePlot(std::string_view id) : _id(id) {}

LinePlot& LinePlot::Size(float w, float h) {
    _width  = w;
    _height = h;
    return *this;
}

LinePlot& LinePlot::XLabel(std::string_view label) {
    _xLabel = label;
    return *this;
}

LinePlot& LinePlot::YLabel(std::string_view label) {
    _yLabel = label;
    return *this;
}

LinePlot& LinePlot::Series(std::string_view name,
                            std::span<const double> xs,
                            std::span<const double> ys) {
    _series.push_back({std::string(name), xs, ys});
    return *this;
}

void LinePlot::Show() {
    PlotScope scope(ImPlot::BeginPlot(_id.c_str(), ImVec2(_width, _height)));
    if (scope) {
        if (!_xLabel.empty() || !_yLabel.empty()) {
            ImPlot::SetupAxes(
                _xLabel.empty() ? nullptr : _xLabel.c_str(),
                _yLabel.empty() ? nullptr : _yLabel.c_str()
            );
        }
        for (const auto& s : _series) {
            const int count = static_cast<int>(std::min(s.xs.size(), s.ys.size()));
            ImPlot::PlotLine(s.name.c_str(), s.xs.data(), s.ys.data(), count);
        }
    }
    _series.clear();
}

// ─── BarPlot ──────────────────────────────────────────────────────────────────

BarPlot::BarPlot(std::string_view id) : _id(id) {}

BarPlot& BarPlot::Size(float w, float h) {
    _width  = w;
    _height = h;
    return *this;
}

BarPlot& BarPlot::Bars(std::string_view name,
                        std::span<const double> values,
                        double barWidth) {
    _seriesName = name;
    _values     = values;
    _barWidth   = barWidth;
    return *this;
}

BarPlot& BarPlot::Horizontal(bool horizontal) {
    _horizontal = horizontal;
    return *this;
}

void BarPlot::Show() {
    PlotScope scope(ImPlot::BeginPlot(_id.c_str(), ImVec2(_width, _height)));
    if (scope) {
        const int count = static_cast<int>(_values.size());
        ImPlotSpec spec;
        if (_horizontal) {
            spec.Flags = static_cast<ImPlotItemFlags>(ImPlotBarsFlags_Horizontal);
        }
        ImPlot::PlotBars(_seriesName.c_str(), _values.data(), count, _barWidth, 0.0, spec);
    }
}

// ─── ScatterPlot ──────────────────────────────────────────────────────────────

ScatterPlot::ScatterPlot(std::string_view id) : _id(id) {}

ScatterPlot& ScatterPlot::Size(float w, float h) {
    _width  = w;
    _height = h;
    return *this;
}

ScatterPlot& ScatterPlot::Points(std::string_view name,
                                  std::span<const double> xs,
                                  std::span<const double> ys) {
    _points.push_back({std::string(name), xs, ys});
    return *this;
}

ScatterPlot& ScatterPlot::MarkerSize(float size) {
    _markerSize = size;
    return *this;
}

void ScatterPlot::Show() {
    PlotScope scope(ImPlot::BeginPlot(_id.c_str(), ImVec2(_width, _height)));
    if (scope) {
        ImPlotSpec spec;
        spec.MarkerSize = _markerSize;
        for (const auto& p : _points) {
            const int count = static_cast<int>(std::min(p.xs.size(), p.ys.size()));
            ImPlot::PlotScatter(p.name.c_str(), p.xs.data(), p.ys.data(), count, spec);
        }
    }
    _points.clear();
}

// ─── HeatMap ──────────────────────────────────────────────────────────────────

HeatMap::HeatMap(std::string_view id) : _id(id) {}

HeatMap& HeatMap::Size(float w, float h) {
    _width  = w;
    _height = h;
    return *this;
}

HeatMap& HeatMap::Data(std::span<const double> data, int rows, int cols) {
    _data = data;
    _rows = rows;
    _cols = cols;
    return *this;
}

HeatMap& HeatMap::ColorMap(PlotColormap colormap) {
    _colormap = colormap;
    return *this;
}

HeatMap& HeatMap::ScaleMin(double min) {
    _scaleMin = min;
    return *this;
}

HeatMap& HeatMap::ScaleMax(double max) {
    _scaleMax = max;
    return *this;
}

void HeatMap::Show() {
    ImPlot::PushColormap(static_cast<ImPlotColormap>(static_cast<int>(_colormap)));
    PlotScope scope(ImPlot::BeginPlot(_id.c_str(), ImVec2(_width, _height),
                                      ImPlotFlags_NoLegend));
    if (scope) {
        ImPlot::PlotHeatmap(_id.c_str(), _data.data(), _rows, _cols,
                            _scaleMin, _scaleMax);
    }
    ImPlot::PopColormap();
}

} // namespace ImFrame::Widgets
