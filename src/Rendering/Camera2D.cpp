/**
 * @file     Camera2D.cpp
 * @brief    Camera2D pan/zoom/rotate and canvas–screen coordinate conversion
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-28
 * @version  2.0.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Rendering/Camera2D.hpp"

#include <algorithm>
#include <cmath>

namespace ImFrame::Rendering {

// ─── Setters ──────────────────────────────────────────────────────────────────

Camera2D& Camera2D::SetPosition(Widgets::Vec2 pos) noexcept {
    _position = pos;
    return *this;
}

Camera2D& Camera2D::SetZoom(float zoom) noexcept {
    _zoom = std::clamp(zoom, _minZoom, _maxZoom);
    return *this;
}

Camera2D& Camera2D::SetRotation(float radians) noexcept {
    _rotation = radians;
    return *this;
}

Camera2D& Camera2D::MinZoom(float minZoom) noexcept {
    _minZoom = minZoom;
    _zoom    = std::clamp(_zoom, _minZoom, _maxZoom);
    return *this;
}

Camera2D& Camera2D::MaxZoom(float maxZoom) noexcept {
    _maxZoom = maxZoom;
    _zoom    = std::clamp(_zoom, _minZoom, _maxZoom);
    return *this;
}

// ─── Accessors ────────────────────────────────────────────────────────────────

Widgets::Vec2 Camera2D::GetPosition() const noexcept { return _position; }
float          Camera2D::GetZoom()     const noexcept { return _zoom; }
float          Camera2D::GetRotation() const noexcept { return _rotation; }
float          Camera2D::GetMinZoom()  const noexcept { return _minZoom; }
float          Camera2D::GetMaxZoom()  const noexcept { return _maxZoom; }

// ─── Coordinate conversion ────────────────────────────────────────────────────

Widgets::Vec2 Camera2D::CanvasToScreen(Widgets::Vec2 canvasPos,
                                        Widgets::Vec2 viewportOrigin) const noexcept {
    const float dx = (canvasPos.x - _position.x) * _zoom;
    const float dy = (canvasPos.y - _position.y) * _zoom;

    if (_rotation == 0.0f) {
        return {viewportOrigin.x + dx, viewportOrigin.y + dy};
    }

    const float cosA = std::cos(_rotation);
    const float sinA = std::sin(_rotation);
    return {
        viewportOrigin.x + dx * cosA - dy * sinA,
        viewportOrigin.y + dx * sinA + dy * cosA
    };
}

Widgets::Vec2 Camera2D::ScreenToCanvas(Widgets::Vec2 screenPos,
                                        Widgets::Vec2 viewportOrigin) const noexcept {
    float dx = (screenPos.x - viewportOrigin.x) / _zoom;
    float dy = (screenPos.y - viewportOrigin.y) / _zoom;

    if (_rotation == 0.0f) {
        return {_position.x + dx, _position.y + dy};
    }

    // Inverse rotation: rotate by -_rotation
    const float cosA =  std::cos(_rotation);
    const float sinA = -std::sin(_rotation);
    return {
        _position.x + dx * cosA - dy * sinA,
        _position.y + dx * sinA + dy * cosA
    };
}

// ─── Operations ───────────────────────────────────────────────────────────────

void Camera2D::Reset() noexcept {
    _position = {0.0f, 0.0f};
    _zoom     = 1.0f;
    _rotation = 0.0f;
}

void Camera2D::FitToRect(Widgets::Vec2 min, Widgets::Vec2 max,
                          Widgets::Vec2 viewportSize) noexcept {
    const float rectW = max.x - min.x;
    const float rectH = max.y - min.y;

    if (rectW <= 0.f || rectH <= 0.f || viewportSize.x <= 0.f || viewportSize.y <= 0.f) {
        return;
    }

    const float zoomX = viewportSize.x / rectW;
    const float zoomY = viewportSize.y / rectH;
    // 0.9 margin so the rect isn't flush with the viewport edge
    _zoom = std::clamp(std::min(zoomX, zoomY) * 0.9f, _minZoom, _maxZoom);

    // _position is the canvas point at the viewport's top-left corner.
    // Center the rect: rect_center = _position + viewportSize/2 / zoom
    // => _position = rect_center - viewportSize/2 / zoom
    const float centerX = (min.x + max.x) * 0.5f;
    const float centerY = (min.y + max.y) * 0.5f;
    _position.x = centerX - (viewportSize.x * 0.5f) / _zoom;
    _position.y = centerY - (viewportSize.y * 0.5f) / _zoom;
}

} // namespace ImFrame::Rendering
