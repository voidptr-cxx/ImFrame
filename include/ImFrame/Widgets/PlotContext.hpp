/**
 * @file     PlotContext.hpp
 * @brief    ImPlot context owner and per-frame theme bridge for plot widgets
 *
 * `PlotContext` owns the `ImPlotContext` created by `ImPlot::CreateContext()`.
 * It is initialised by `Application::Run()` after the ImGui context exists and
 * shut down before `IBackend::Shutdown()` destroys the ImGui context.
 *
 * `PlotColormap` mirrors the `ImPlotColormap` integer enum without pulling
 * `<implot.h>` into the public API. Values match ImPlot's built-in colormaps.
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

// Forward-declare to avoid dragging <implot.h> into the public API.
struct ImPlotContext;

namespace ImFrame::Theme { struct Theme; }

namespace ImFrame::Widgets {

// ─── PlotColormap ─────────────────────────────────────────────────────────────

/**
 * @brief    Built-in ImPlot colormaps available for heat-map widgets
 *
 * Integer values match `ImPlotColormap` exactly; cast-safe to use with
 * `ImPlot::PushColormap()` in the implementation translation unit.
 *
 * @since    1.5.0
 */
enum class PlotColormap : int {
    Deep    = 0, ///< ImPlot default — qualitative (9 colors)
    Dark    = 1, ///< Dark qualitative (9 colors)
    Pastel  = 2, ///< Pastel qualitative (9 colors)
    Paired  = 3, ///< Paired qualitative (12 colors)
    Viridis = 4, ///< Viridis sequential (11 colors)
    Plasma  = 5, ///< Plasma sequential (11 colors)
    Hot     = 6, ///< Hot sequential (11 colors)
    Cool    = 7, ///< Cool diverging (11 colors)
    Pink    = 8, ///< Pink sequential (11 colors)
    Jet     = 9, ///< Jet diverging (11 colors)
};

// ─── PlotContext ──────────────────────────────────────────────────────────────

/**
 * @class    PlotContext
 * @brief    Owns the ImPlot context and applies ImFrame theme tokens to ImPlot style
 *
 * Managed exclusively by `Application`. Callers must not create additional
 * `PlotContext` instances — one context per ImGui context is an ImPlot invariant.
 *
 * @note     Non-copyable, non-moveable.
 *
 * @since    1.5.0
 *
 * @example
 * @code
 * // Managed automatically by Application — no user-side setup required.
 * app.WithTheme(ImFrame::Themes::Dracula);
 * app.OnUi([] {
 *     LinePlot("##demo")
 *         .Series("sin", xs, ys)
 *         .Show();
 * });
 * app.Run();
 * @endcode
 */
class PlotContext {
public:
    PlotContext()  = default;
    ~PlotContext() = default;

    PlotContext(const PlotContext&)            = delete;
    PlotContext& operator=(const PlotContext&) = delete;
    PlotContext(PlotContext&&)                 = delete;
    PlotContext& operator=(PlotContext&&)      = delete;

    /**
     * @brief    Create the ImPlot context. Call after ImGui::CreateContext().
     * @throws   Nothing.
     */
    void Init();

    /**
     * @brief    Destroy the ImPlot context. Call before ImGui::DestroyContext().
     * @throws   Nothing.
     */
    void Shutdown();

    /**
     * @brief    Map ImFrame theme tokens to ImPlot style slots.
     *
     * Called every frame from `Application::RunOneFrame()` because ImPlot style
     * is reset on context recreation and must be re-applied to stay in sync with
     * dynamic theme changes.
     *
     * Mapping:
     * - `accentDefault`       → `ImPlotCol_Line`
     * - `backgroundSecondary` → `ImPlotCol_PlotBg`
     * - `surfaceDefault`      → `ImPlotCol_FrameBg`
     * - `textSecondary`       → `ImPlotCol_AxisText`
     *
     * @param[in]  theme  Active ImFrame theme.
     * @throws   Nothing.
     */
    void ApplyTheme(const ImFrame::Theme::Theme& theme);

private:
    ImPlotContext* _ctx = nullptr;
};

} // namespace ImFrame::Widgets
