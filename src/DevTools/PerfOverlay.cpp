/**
 * @file     PerfOverlay.cpp
 * @brief    Performance overlay — frame time, FPS rolling average, ImGui vertex/index metrics
 *
 * @internal
 * Position is computed each frame from the viewport size and the configured
 * corner + padding. `ImGui::SetNextWindowPos` with `ImGuiCond_Always` pins the
 * overlay; `ImGuiWindowFlags_NoMove` prevents the user from dragging it.
 *
 * FPS is derived from the 60-frame rolling window of delta times; the reciprocal
 * of the average delta gives the mean FPS over the last second. The raw values
 * are fed into `AnimatedValue<float>` for display smoothing (speed = 5).
 *
 * Vertex and index counts come from `ImGui::GetIO().MetricsRenderVertices` and
 * `MetricsRenderIndices`, which hold the previous frame's draw statistics.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-13
 * @version  1.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#if defined(IMF_DEV_TOOLS)

#include "ImFrame/DevTools/PerfOverlay.hpp"

#include <imgui.h>

#include <numeric>

namespace ImFrame::DevTools {

// ─── Construction ─────────────────────────────────────────────────────────────

PerfOverlay::PerfOverlay()
    : _smoothFps(60.0f, 5.0f)
    , _smoothFrameMs(16.667f, 5.0f)
{}

// ─── Configuration ────────────────────────────────────────────────────────────

void PerfOverlay::SetConfig(PerfOverlayConfig config) noexcept
{
    _config = config;
}

// ─── Render ───────────────────────────────────────────────────────────────────

void PerfOverlay::Render(float deltaTime)
{
    // ── Update rolling frame-time buffer ──────────────────────────────────────
    _frameTimes[_frameIdx] = deltaTime > 0.0f ? deltaTime : 1e-6f;
    _frameIdx = (_frameIdx + 1) % ROLLING_WINDOW;
    if (_frameCount < ROLLING_WINDOW) ++_frameCount;

    const int count = _frameCount;
    float sum = 0.0f;
    for (int i = 0; i < count; ++i) {
        sum += _frameTimes[i];
    }
    const float avgDt = sum / static_cast<float>(count);
    const float rawFps = 1.0f / avgDt;
    const float rawMs  = avgDt * 1000.0f;

    _smoothFps.SetTarget(rawFps);
    _smoothFps.Update(deltaTime);

    _smoothFrameMs.SetTarget(rawMs);
    _smoothFrameMs.Update(deltaTime);

    // ── Compute corner position ────────────────────────────────────────────
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    // In headless / zero-size contexts there is nothing to anchor to.
    if (!vp || (vp->WorkSize.x == 0.0f && vp->WorkSize.y == 0.0f)) return;
    const float pad = _config.padding;
    ImVec2 windowPos{};
    ImVec2 windowPivot{};

    switch (_config.corner) {
        case OverlayCorner::TopLeft:
            windowPos   = {vp->WorkPos.x + pad, vp->WorkPos.y + pad};
            windowPivot = {0.0f, 0.0f};
            break;
        case OverlayCorner::TopRight:
            windowPos   = {vp->WorkPos.x + vp->WorkSize.x - pad, vp->WorkPos.y + pad};
            windowPivot = {1.0f, 0.0f};
            break;
        case OverlayCorner::BottomLeft:
            windowPos   = {vp->WorkPos.x + pad, vp->WorkPos.y + vp->WorkSize.y - pad};
            windowPivot = {0.0f, 1.0f};
            break;
        case OverlayCorner::BottomRight:
            windowPos   = {vp->WorkPos.x + vp->WorkSize.x - pad,
                           vp->WorkPos.y + vp->WorkSize.y - pad};
            windowPivot = {1.0f, 1.0f};
            break;
    }

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, windowPivot);
    ImGui::SetNextWindowBgAlpha(_config.alpha);

    const ImGuiWindowFlags OVERLAY_FLAGS =
        ImGuiWindowFlags_NoDecoration        |
        ImGuiWindowFlags_AlwaysAutoResize    |
        ImGuiWindowFlags_NoSavedSettings     |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoFocusOnAppearing  |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoDocking;

    if (!ImGui::Begin("##PerfOverlay", nullptr, OVERLAY_FLAGS)) {
        ImGui::End();
        return;
    }

    const auto& io = ImGui::GetIO();

    ImGui::Text("%.2f ms / frame", _smoothFrameMs.Value());
    ImGui::Text("%.1f FPS", _smoothFps.Value());
    ImGui::Separator();
    ImGui::Text("Verts: %d", io.MetricsRenderVertices);
    ImGui::Text("Idx:   %d", io.MetricsRenderIndices);

    ImGui::End();
}

} // namespace ImFrame::DevTools

#endif // defined(IMF_DEV_TOOLS)
