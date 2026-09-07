/**
 * @file     Camera2D.hpp
 * @brief    2D camera with position, zoom, and rotation for use with Canvas2D
 *
 * `_position` is the canvas-space point that maps to the viewport's top-left corner.
 * `CanvasToScreen` / `ScreenToCanvas` require the viewport origin at call time so
 * the canvas can be embedded at any screen position.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-28
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Widgets/Types.hpp"

namespace ImFrame::Rendering {

/**
 * @class Camera2D
 * @brief  Defines which canvas region is visible and at what zoom and rotation
 *
 * `_position` is the canvas point at the viewport's top-left corner.
 * Zoom is pixels-per-canvas-unit. Rotation is counter-clockwise in radians.
 *
 * @since  2.0.0
 *
 * @example
 * @code
 * Camera2D cam;
 * cam.SetZoom(2.0f);
 * auto screen = cam.CanvasToScreen({50.f, 50.f}, origin); // origin + (100, 100)
 * @endcode
 */
class Camera2D {
public:
    // ─── Setters ──────────────────────────────────────────────────────────────

    /**
     * @brief  Set the canvas-space coordinate visible at the viewport's top-left corner.
     * @param[in] pos  Canvas position.
     * @return  Reference to this Camera2D.
     */
    Camera2D& SetPosition(Widgets::Vec2 pos) noexcept;

    /**
     * @brief  Set the zoom level (pixels per canvas unit). Clamped to [MinZoom, MaxZoom].
     * @param[in] zoom  Zoom factor; must be > 0.
     * @return  Reference to this Camera2D.
     */
    Camera2D& SetZoom(float zoom) noexcept;

    /**
     * @brief  Set the view rotation in radians (counter-clockwise).
     * @param[in] radians  Rotation angle.
     * @return  Reference to this Camera2D.
     */
    Camera2D& SetRotation(float radians) noexcept;

    /**
     * @brief  Set the minimum allowed zoom. Default: 0.1.
     * @param[in] minZoom  Minimum zoom (> 0).
     * @return  Reference to this Camera2D.
     */
    Camera2D& MinZoom(float minZoom) noexcept;

    /**
     * @brief  Set the maximum allowed zoom. Default: 10.0.
     * @param[in] maxZoom  Maximum zoom (> MinZoom).
     * @return  Reference to this Camera2D.
     */
    Camera2D& MaxZoom(float maxZoom) noexcept;

    // ─── Accessors ────────────────────────────────────────────────────────────

    [[nodiscard]] Widgets::Vec2 GetPosition() const noexcept;  ///< Canvas-space viewport origin.
    [[nodiscard]] float          GetZoom()     const noexcept;  ///< Current zoom (pixels/canvas-unit).
    [[nodiscard]] float          GetRotation() const noexcept;  ///< Current rotation (radians, CCW).
    [[nodiscard]] float          GetMinZoom()  const noexcept;  ///< Minimum clamped zoom.
    [[nodiscard]] float          GetMaxZoom()  const noexcept;  ///< Maximum clamped zoom.

    // ─── Coordinate conversion ────────────────────────────────────────────────

    /**
     * @brief  Convert a canvas-space point to screen pixels.
     * @param[in] canvasPos       Point in canvas-space.
     * @param[in] viewportOrigin  Top-left corner of the canvas in screen pixels.
     * @return  Screen-space coordinates.
     * @throws  Nothing — noexcept
     */
    [[nodiscard]] Widgets::Vec2 CanvasToScreen(Widgets::Vec2 canvasPos,
                                                Widgets::Vec2 viewportOrigin) const noexcept;

    /**
     * @brief  Convert screen pixels back to canvas-space.
     * @param[in] screenPos       Point in screen pixels.
     * @param[in] viewportOrigin  Top-left corner of the canvas in screen pixels.
     * @return  Canvas-space coordinates.
     * @throws  Nothing — noexcept
     */
    [[nodiscard]] Widgets::Vec2 ScreenToCanvas(Widgets::Vec2 screenPos,
                                                Widgets::Vec2 viewportOrigin) const noexcept;

    // ─── Operations ───────────────────────────────────────────────────────────

    /**
     * @brief  Reset to origin, zoom 1, and no rotation.
     */
    void Reset() noexcept;

    /**
     * @brief  Adjust position and zoom to frame a canvas-space rectangle with 10% margin.
     *
     * Has no effect if the rectangle has zero area or `viewportSize` is zero.
     *
     * @param[in] min           Minimum corner (canvas-space).
     * @param[in] max           Maximum corner (canvas-space).
     * @param[in] viewportSize  Viewport dimensions in screen pixels.
     */
    void FitToRect(Widgets::Vec2 min, Widgets::Vec2 max, Widgets::Vec2 viewportSize) noexcept;

private:
    Widgets::Vec2 _position = {0.0f, 0.0f};
    float         _zoom     = 1.0f;
    float         _rotation = 0.0f;
    float         _minZoom  = 0.1f;
    float         _maxZoom  = 10.0f;
};

} // namespace ImFrame::Rendering
