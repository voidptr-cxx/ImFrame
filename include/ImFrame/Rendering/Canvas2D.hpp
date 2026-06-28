/**
 * @file     Canvas2D.hpp
 * @brief    2D canvas widget with layers, camera, hit-testing, and static subtree caching
 *
 * Canvas2D renders into an ImGui child window using ImDrawList primitives. Each call to
 * `Show()` creates the child window, runs the `OnDraw` callback, composites layers in
 * ascending index order, and fires `OnHit` if the user clicked a hit-testable element.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-28
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Rendering/Camera2D.hpp"
#include "ImFrame/Rendering/DrawContext.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace ImFrame::Internal { struct CanvasImpl; }

namespace ImFrame::Rendering {

/**
 * @enum  CanvasInteractionMode
 * @brief Controls how Canvas2D responds to mouse input
 * @since 2.0.0
 */
enum class CanvasInteractionMode : uint8_t {
    None,     ///< Camera is not driven by mouse input.
    Pan,      ///< Left-button drag or middle-button drag pans the camera.
    PanZoom   ///< Pan plus scroll-wheel zoom toward the cursor.
};

/**
 * @class Canvas2D
 * @brief  2D scene canvas widget with layers, Camera2D, hit testing, and static caching
 *
 * Follows the Phase 10–14 builder pattern: construct, configure with fluent setters,
 * then call `Show()` once per frame from the `OnUi` callback.
 *
 * @since  2.0.0
 *
 * @example
 * @code
 * Rendering::Canvas2D("my_canvas")
 *     .Size({800.f, 600.f})
 *     .InteractionMode(CanvasInteractionMode::PanZoom)
 *     .OnDraw([](Rendering::DrawContext& ctx) {
 *         ctx.DrawRectFilled({-50,-50}, {50,50}, {0.3f,0.6f,1.f,1.f});
 *     })
 *     .Show();
 * @endcode
 */
class Canvas2D {
public:
    /**
     * @brief  Construct a Canvas2D with a stable per-canvas ID.
     * @param[in] id  Unique identifier used as the ImGui child-window ID.
     *                Must be stable across frames for the same logical canvas.
     */
    explicit Canvas2D(std::string_view id);

    ~Canvas2D() noexcept;

    Canvas2D(const Canvas2D&)            = delete;
    Canvas2D& operator=(const Canvas2D&) = delete;
    Canvas2D(Canvas2D&&)                 noexcept;
    Canvas2D& operator=(Canvas2D&&)      noexcept;

    // ─── Builder setters ──────────────────────────────────────────────────────

    /**
     * @brief  Pin the canvas to a fixed pixel size.
     *
     * When not called, the canvas fills the parent window's content region.
     *
     * @param[in] size  Pixel dimensions; both components must be > 0.
     * @return  Reference to this Canvas2D for chaining.
     */
    Canvas2D& Size(Widgets::Vec2 size);

    /**
     * @brief  Set the background fill colour. Defaults to a dark grey.
     * @param[in] color  RGBA background colour.
     * @return  Reference to this Canvas2D for chaining.
     */
    Canvas2D& Background(Widgets::Vec4 color);

    /**
     * @brief  Set how the canvas responds to mouse input.
     * @param[in] mode  Interaction mode.
     * @return  Reference to this Canvas2D for chaining.
     */
    Canvas2D& InteractionMode(CanvasInteractionMode mode);

    /**
     * @brief  Register the draw callback invoked once per `Show()`.
     * @param[in] callback  Callable accepting a `DrawContext&`.
     * @return  Reference to this Canvas2D for chaining.
     */
    Canvas2D& OnDraw(Utility::Delegate<void(DrawContext&)> callback);

    /**
     * @brief  Register a callback fired when the user clicks a hit-testable element.
     * @param[in] callback  Callable accepting the hit-target ID and canvas-space position.
     * @return  Reference to this Canvas2D for chaining.
     */
    Canvas2D& OnHit(Utility::Delegate<void(uint32_t, Widgets::Vec2)> callback);

    // ─── Camera ───────────────────────────────────────────────────────────────

    /**
     * @brief  Set the minimum zoom for PanZoom mode. Default: 0.1.
     * @param[in] minZoom  Minimum zoom (> 0).
     * @return  Reference to this Canvas2D for chaining.
     */
    Canvas2D& MinZoom(float minZoom);

    /**
     * @brief  Set the maximum zoom for PanZoom mode. Default: 10.0.
     * @param[in] maxZoom  Maximum zoom.
     * @return  Reference to this Canvas2D for chaining.
     */
    Canvas2D& MaxZoom(float maxZoom);

    /**
     * @brief  Reset the camera to canvas-space origin, zoom 1, no rotation.
     */
    void ResetCamera();

    /**
     * @brief  Adjust the camera to fit a canvas-space rectangle into view.
     *
     * Uses the viewport size recorded from the most recent `Show()` call.
     * Has no effect before the first `Show()`.
     *
     * @param[in] min  Minimum corner (canvas-space).
     * @param[in] max  Maximum corner (canvas-space).
     */
    void FitToRect(Widgets::Vec2 min, Widgets::Vec2 max);

    /**
     * @brief  Read-only access to the camera for inspection.
     * @return  Const reference to the internal Camera2D.
     */
    [[nodiscard]] const Camera2D& GetCamera() const noexcept;

    // ─── Hit testing ──────────────────────────────────────────────────────────

    /**
     * @brief  Query the topmost hit-testable shape under a screen position.
     *
     * Valid to call after `Show()`. Shapes on higher layers occlude lower ones.
     *
     * @param[in] screenPos  Position in screen pixels.
     * @return  Hit-target ID of the topmost shape; `std::nullopt` if none.
     */
    [[nodiscard]] std::optional<uint32_t> HitTest(Widgets::Vec2 screenPos) const;

    // ─── Layer management ─────────────────────────────────────────────────────

    /**
     * @brief  Set a layer's visibility.
     * @param[in] layerIndex  Layer to configure.
     * @param[in] visible     `true` to render the layer; `false` to skip it.
     */
    void SetLayerVisible(int layerIndex, bool visible);

    /**
     * @brief  Set a layer's global opacity (applied via ImDrawList global alpha).
     * @param[in] layerIndex  Layer to configure.
     * @param[in] opacity     Opacity in [0, 1].
     */
    void SetLayerOpacity(int layerIndex, float opacity);

    /// @return Number of distinct layers used in the most recent `Show()` call.
    [[nodiscard]] int LayerCount() const noexcept;

    // ─── Terminal ─────────────────────────────────────────────────────────────

    /**
     * @brief  Composite this canvas into the current ImGui window for one frame.
     *
     * Calls `OnDraw`, composites layers in ascending index order, runs pan/zoom
     * interaction, and fires `OnHit` on a click hit.
     *
     * @return  `true` if a hit-testable element was clicked during this frame.
     */
    bool Show();

private:
    std::unique_ptr<Internal::CanvasImpl> _impl;
};

} // namespace ImFrame::Rendering
