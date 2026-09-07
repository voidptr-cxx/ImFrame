/**
 * @file     DrawContext.hpp
 * @brief    Canvas-space drawing interface passed to Canvas2D::OnDraw callbacks
 *
 * All coordinates accepted by draw methods are in canvas-space. The active camera
 * transform and any PushTransform entries are applied internally before issuing
 * draw commands. A layer stack lets callers route draws to specific compositing
 * layers; a transform stack applies additional affine transforms on top of the camera.
 *
 * DrawContext instances are created and owned by Canvas2D — they are never
 * constructed directly by application code.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-28
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Rendering/Transform2D.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <cstdint>
#include <optional>

namespace ImFrame::Internal { struct DrawContextImpl; }

namespace ImFrame::Rendering {

/**
 * @class DrawContext
 * @brief  Stateful canvas-space drawing interface for use inside OnDraw callbacks
 *
 * Passed by mutable reference to the `OnDraw` callback registered with `Canvas2D`.
 * Lower layer indices composite first (appear behind higher indices). Transform
 * stack entries accumulate multiplicatively; `PopTransform()` undoes the most
 * recently pushed entry.
 *
 * @since  2.0.0
 *
 * @example
 * @code
 * canvas.OnDraw([](Rendering::DrawContext& ctx) {
 *     ctx.PushLayer(0);
 *     ctx.DrawRectFilled({-50,-50}, {50,50}, {0.2f,0.2f,0.2f,1.f});
 *     ctx.PopLayer();
 *     ctx.PushLayer(1);
 *     ctx.DrawCircle({0,0}, 30.f, {1.f,0.3f,0.f,1.f});
 *     ctx.PopLayer();
 * });
 * @endcode
 */
class DrawContext {
public:
    // ─── Primitives ───────────────────────────────────────────────────────────

    /**
     * @brief  Draw a line segment.
     * @param[in] p0        Start point (canvas-space).
     * @param[in] p1        End point (canvas-space).
     * @param[in] color     RGBA colour.
     * @param[in] thickness Line width in screen pixels.
     * @param[in] hitTarget Optional hit-test ID (0 = not hit-testable).
     */
    void DrawLine(Widgets::Vec2 p0, Widgets::Vec2 p1, Widgets::Vec4 color,
                  float thickness = 1.0f, uint32_t hitTarget = 0);

    /**
     * @brief  Draw an unfilled rectangle outline.
     * @param[in] min       Top-left corner (canvas-space).
     * @param[in] max       Bottom-right corner (canvas-space).
     * @param[in] color     RGBA colour.
     * @param[in] rounding  Corner radius in canvas units.
     * @param[in] thickness Line width in screen pixels.
     * @param[in] hitTarget Optional hit-test ID.
     */
    void DrawRect(Widgets::Vec2 min, Widgets::Vec2 max, Widgets::Vec4 color,
                  float rounding = 0.0f, float thickness = 1.0f, uint32_t hitTarget = 0);

    /**
     * @brief  Draw a filled rectangle.
     * @param[in] min       Top-left corner (canvas-space).
     * @param[in] max       Bottom-right corner (canvas-space).
     * @param[in] color     RGBA colour.
     * @param[in] rounding  Corner radius in canvas units.
     * @param[in] hitTarget Optional hit-test ID.
     */
    void DrawRectFilled(Widgets::Vec2 min, Widgets::Vec2 max, Widgets::Vec4 color,
                        float rounding = 0.0f, uint32_t hitTarget = 0);

    /**
     * @brief  Draw an unfilled circle.
     * @param[in] center    Centre (canvas-space).
     * @param[in] radius    Radius in canvas units.
     * @param[in] color     RGBA colour.
     * @param[in] thickness Line width in screen pixels.
     * @param[in] hitTarget Optional hit-test ID.
     */
    void DrawCircle(Widgets::Vec2 center, float radius, Widgets::Vec4 color,
                    float thickness = 1.0f, uint32_t hitTarget = 0);

    /**
     * @brief  Draw a filled circle.
     * @param[in] center    Centre (canvas-space).
     * @param[in] radius    Radius in canvas units.
     * @param[in] color     RGBA colour.
     * @param[in] hitTarget Optional hit-test ID.
     */
    void DrawCircleFilled(Widgets::Vec2 center, float radius, Widgets::Vec4 color,
                          uint32_t hitTarget = 0);

    /**
     * @brief  Draw an unfilled ellipse.
     * @param[in] center    Centre (canvas-space).
     * @param[in] radius    Half-extents (rx, ry) in canvas units.
     * @param[in] color     RGBA colour.
     * @param[in] thickness Line width in screen pixels.
     * @param[in] hitTarget Optional hit-test ID.
     */
    void DrawEllipse(Widgets::Vec2 center, Widgets::Vec2 radius, Widgets::Vec4 color,
                     float thickness = 1.0f, uint32_t hitTarget = 0);

    /**
     * @brief  Draw a polyline or closed polygon outline.
     * @param[in] points    Pointer to canvas-space points.
     * @param[in] count     Number of points.
     * @param[in] color     RGBA colour.
     * @param[in] closed    Connect the last point back to the first.
     * @param[in] thickness Line width in screen pixels.
     * @param[in] hitTarget Optional hit-test ID.
     */
    void DrawPolyline(const Widgets::Vec2* points, int count, Widgets::Vec4 color,
                      bool closed = false, float thickness = 1.0f, uint32_t hitTarget = 0);

    /**
     * @brief  Draw a cubic Bézier curve.
     * @param[in] p0        Start point (canvas-space).
     * @param[in] p1        First control point (canvas-space).
     * @param[in] p2        Second control point (canvas-space).
     * @param[in] p3        End point (canvas-space).
     * @param[in] color     RGBA colour.
     * @param[in] thickness Line width in screen pixels.
     * @param[in] hitTarget Optional hit-test ID.
     */
    void DrawBezierCubic(Widgets::Vec2 p0, Widgets::Vec2 p1,
                         Widgets::Vec2 p2, Widgets::Vec2 p3,
                         Widgets::Vec4 color, float thickness = 1.0f, uint32_t hitTarget = 0);

    /**
     * @brief  Draw a text string at a canvas-space position.
     * @param[in] pos       Baseline-left position (canvas-space).
     * @param[in] color     RGBA colour.
     * @param[in] text      Null-terminated UTF-8 string.
     * @param[in] fontSize  Desired font size in canvas units; 0 uses the ImGui default.
     */
    void DrawText(Widgets::Vec2 pos, Widgets::Vec4 color, const char* text,
                  float fontSize = 0.0f);

    /**
     * @brief  Draw a GPU texture within a canvas-space rectangle.
     * @param[in] textureId Backend texture handle.
     * @param[in] min       Top-left corner (canvas-space).
     * @param[in] max       Bottom-right corner (canvas-space).
     * @param[in] uv0       Top-left UV coordinate.
     * @param[in] uv1       Bottom-right UV coordinate.
     * @param[in] tint      RGBA tint colour.
     */
    void DrawImage(Widgets::TextureHandle textureId,
                   Widgets::Vec2 min, Widgets::Vec2 max,
                   Widgets::Vec2 uv0 = {0.f, 0.f}, Widgets::Vec2 uv1 = {1.f, 1.f},
                   Widgets::Vec4 tint = {1.f, 1.f, 1.f, 1.f});

    // ─── Layer stack ──────────────────────────────────────────────────────────

    /**
     * @brief  Direct subsequent draw calls to the given layer.
     *
     * Layers with lower indices composite first (appear behind higher indices).
     * Indices are arbitrary integers; negative values are valid.
     *
     * @param[in] layerIndex  Target layer index.
     */
    void PushLayer(int layerIndex);

    /**
     * @brief  Pop the most recently pushed layer, restoring the previous active layer.
     *
     * Popping past the bottom of the stack is a no-op.
     */
    void PopLayer();

    // ─── Transform stack ──────────────────────────────────────────────────────

    /**
     * @brief  Push an additional affine transform onto the canvas-space transform stack.
     *
     * The new transform is applied BEFORE the current accumulated transform (i.e., in
     * the local space of the current coordinate frame). Nesting is bounded by the
     * stack depth limit of 32 entries.
     *
     * @param[in] transform  Additional canvas-space transform to apply.
     */
    void PushTransform(Transform2D transform);

    /**
     * @brief  Pop the most recently pushed transform, restoring the previous frame.
     *
     * Popping past the bottom of the stack is a no-op.
     */
    void PopTransform();

    // ─── Static subtree caching ───────────────────────────────────────────────

    /**
     * @brief  Begin a cacheable draw block.
     *
     * If `cacheKey` matches the previous frame, the block is replayed from cache and
     * `false` is returned — the caller should skip re-issuing draw commands. If the key
     * changed (or this is the first frame), `true` is returned and the caller must draw.
     *
     * @param[in] cacheKey  Stable key for this static block. Change it to invalidate.
     * @return  `true` if draw commands must be issued; `false` if cache was used.
     */
    bool BeginStatic(uint64_t cacheKey);

    /**
     * @brief  End a cacheable draw block opened by `BeginStatic()`.
     *
     * Records the current block into the cache if it was not a cache hit.
     */
    void EndStatic();

    // ─── Internal — not part of the public API ────────────────────────────────

    /// @internal Called by Canvas2D::Show() to construct a DrawContext for the frame.
    explicit DrawContext(Internal::DrawContextImpl* impl) noexcept;

    DrawContext(const DrawContext&)            = delete;
    DrawContext& operator=(const DrawContext&) = delete;

private:
    Internal::DrawContextImpl* _impl;
};

} // namespace ImFrame::Rendering
