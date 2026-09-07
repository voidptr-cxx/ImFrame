/**
 * @file     PerfOverlay.hpp
 * @brief    Always-on-top performance metrics overlay (frame time, FPS, vertex/index counts)
 *
 * Renders a small non-interactive overlay in a configurable corner of the
 * viewport. Frame time and FPS are smoothed via `AnimatedValue<float>`. Vertex
 * and index counts are read from the previous frame's ImGuiIO metrics.
 *
 * Conditionally compiled: only present when `IMF_DEV_TOOLS` is defined.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-13
 * @version  1.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#if defined(IMF_DEV_TOOLS)

#include "ImFrame/Anim/AnimatedValue.hpp"

#include <array>

namespace ImFrame::DevTools {

/**
 * @enum     OverlayCorner
 * @brief    Screen corner used as the anchor point for the performance overlay
 * @since    1.7.0
 */
enum class OverlayCorner {
    TopLeft,     ///< Upper-left corner
    TopRight,    ///< Upper-right corner (default)
    BottomLeft,  ///< Lower-left corner
    BottomRight, ///< Lower-right corner
};

/**
 * @struct   PerfOverlayConfig
 * @brief    Configuration for performance overlay position and transparency
 * @since    1.7.0
 */
struct PerfOverlayConfig {
    OverlayCorner corner  = OverlayCorner::TopRight; ///< Anchor corner of the overlay
    float         padding = 10.0f;                   ///< Distance in pixels from the corner edge
    float         alpha   = 0.75f;                   ///< Background alpha [0.0, 1.0]
};

/**
 * @class    PerfOverlay
 * @brief    Non-interactive corner overlay showing frame time, FPS, and ImGui vertex metrics
 *
 * Rendered with `ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
 * ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
 * ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToDisplayOnFocus`.
 *
 * FPS is a 60-frame rolling average. Frame time and FPS values are additionally
 * smoothed by `AnimatedValue<float>` with speed 5 to reduce visual jitter.
 *
 * @since    1.7.0
 *
 * @example
 * @code
 * ImFrame::DevTools::PerfOverlay overlay;
 * overlay.SetConfig({.corner = ImFrame::DevTools::OverlayCorner::TopRight, .alpha = 0.8f});
 * app.OnUi([&] { overlay.Render(app.DeltaTime()); });
 * @endcode
 */
class PerfOverlay {
public:
    PerfOverlay();

    /**
     * @brief    Updates the overlay configuration
     * @param[in] config  New settings; applied on the next Render() call.
     */
    void SetConfig(PerfOverlayConfig config) noexcept;

    /**
     * @brief    Renders the overlay; call once per frame from an OnUi callback
     * @param[in] deltaTime  Seconds elapsed since the previous frame.
     */
    void Render(float deltaTime);

private:
    static constexpr int ROLLING_WINDOW = 60;

    PerfOverlayConfig                  _config;
    std::array<float, ROLLING_WINDOW>  _frameTimes{};
    int                                _frameIdx = 0;
    int                                _frameCount = 0;
    Anim::AnimatedValue<float>         _smoothFps;
    Anim::AnimatedValue<float>         _smoothFrameMs;
};

} // namespace ImFrame::DevTools

#endif // defined(IMF_DEV_TOOLS)
