/**
 * @file     DrawContext.cpp
 * @brief    DrawContext draw-primitive implementations
 *
 * All methods pre-compute screen-space coordinates at call time (canvas→transform
 * stack→camera→screen) and enqueue a deferred lambda capturing only the final
 * screen coordinates. Deferred commands are sorted by layer and replayed into
 * the ImGui window draw list by Canvas2D::Show().
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-28
 * @version  2.0.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Rendering/DrawContext.hpp"
#include "DrawContextImpl.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace ImFrame::Rendering {

// ─── Constructor ──────────────────────────────────────────────────────────────

DrawContext::DrawContext(Internal::DrawContextImpl* impl) noexcept
    : _impl{impl}
{}

// ─── Internal helpers ─────────────────────────────────────────────────────────

namespace {

/// Convert Widgets::Vec4 RGBA to ImU32 (0xAABBGGRR little-endian).
static ImU32 ToImU32(Widgets::Vec4 c) {
    const auto r = static_cast<uint8_t>(std::clamp(c.x, 0.f, 1.f) * 255.f + 0.5f);
    const auto g = static_cast<uint8_t>(std::clamp(c.y, 0.f, 1.f) * 255.f + 0.5f);
    const auto b = static_cast<uint8_t>(std::clamp(c.z, 0.f, 1.f) * 255.f + 0.5f);
    const auto a = static_cast<uint8_t>(std::clamp(c.w, 0.f, 1.f) * 255.f + 0.5f);
    return IM_COL32(r, g, b, a);
}

} // anonymous namespace

// ─── Canvas→screen helpers (inline, call before enqueue) ─────────────────────

/// Apply transform stack and camera to a single canvas-space point.
static ImVec2 ToScreen(const Internal::DrawContextImpl& impl, Widgets::Vec2 p) {
    const Widgets::Vec2 tp = impl.transformStack.back().Apply(p);
    const Widgets::Vec2 sp = impl.camera->CanvasToScreen(
        tp, {impl.viewportOrigin.x, impl.viewportOrigin.y});
    return {sp.x, sp.y};
}

/// Scale a canvas-unit length to screen pixels using the current zoom.
static float ScaleLen(const Internal::DrawContextImpl& impl, float canvasLen) {
    return canvasLen * impl.camera->GetZoom();
}

/// Register a screen-space AABB hit record if hitTarget != 0.
static void MaybeAddHit(Internal::DrawContextImpl& impl, int layer, uint32_t hitTarget,
                         ImVec2 sMin, ImVec2 sMax) {
    if (hitTarget == 0) return;
    impl.hitRecords->push_back({layer, hitTarget, sMin, sMax});
}

// ─── Enqueue helper ───────────────────────────────────────────────────────────

template<typename Fn>
static void Enqueue(Internal::DrawContextImpl& impl, Fn&& fn) {
    impl.deferred->push_back({impl.layerStack.back(), std::forward<Fn>(fn)});
}

// ─── Primitives ───────────────────────────────────────────────────────────────

void DrawContext::DrawLine(Widgets::Vec2 p0, Widgets::Vec2 p1, Widgets::Vec4 color,
                           float thickness, uint32_t hitTarget) {
    auto& im   = *_impl;
    ImVec2 sp0 = ToScreen(im, p0);
    ImVec2 sp1 = ToScreen(im, p1);
    ImU32  col = ToImU32(color);
    float  th  = thickness;
    int    lay = im.layerStack.back();

    MaybeAddHit(im, lay, hitTarget,
                {std::min(sp0.x, sp1.x) - th, std::min(sp0.y, sp1.y) - th},
                {std::max(sp0.x, sp1.x) + th, std::max(sp0.y, sp1.y) + th});

    Enqueue(im, [sp0, sp1, col, th](ImDrawList* dl) {
        dl->AddLine(sp0, sp1, col, th);
    });
}

void DrawContext::DrawRect(Widgets::Vec2 min, Widgets::Vec2 max, Widgets::Vec4 color,
                           float rounding, float thickness, uint32_t hitTarget) {
    auto& im   = *_impl;
    ImVec2 sMin = ToScreen(im, min);
    ImVec2 sMax = ToScreen(im, max);
    ImU32  col  = ToImU32(color);
    float  r    = ScaleLen(im, rounding);
    float  th   = thickness;
    int    lay  = im.layerStack.back();

    MaybeAddHit(im, lay, hitTarget, sMin, sMax);

    Enqueue(im, [sMin, sMax, col, r, th](ImDrawList* dl) {
        dl->AddRect(sMin, sMax, col, r, 0, th);
    });
}

void DrawContext::DrawRectFilled(Widgets::Vec2 min, Widgets::Vec2 max, Widgets::Vec4 color,
                                 float rounding, uint32_t hitTarget) {
    auto& im    = *_impl;
    ImVec2 sMin = ToScreen(im, min);
    ImVec2 sMax = ToScreen(im, max);
    ImU32  col  = ToImU32(color);
    float  r    = ScaleLen(im, rounding);
    int    lay  = im.layerStack.back();

    MaybeAddHit(im, lay, hitTarget, sMin, sMax);

    Enqueue(im, [sMin, sMax, col, r](ImDrawList* dl) {
        dl->AddRectFilled(sMin, sMax, col, r);
    });
}

void DrawContext::DrawCircle(Widgets::Vec2 center, float radius, Widgets::Vec4 color,
                             float thickness, uint32_t hitTarget) {
    auto& im    = *_impl;
    ImVec2 sc   = ToScreen(im, center);
    float  sr   = ScaleLen(im, radius);
    ImU32  col  = ToImU32(color);
    float  th   = thickness;
    int    lay  = im.layerStack.back();

    MaybeAddHit(im, lay, hitTarget, {sc.x - sr, sc.y - sr}, {sc.x + sr, sc.y + sr});

    Enqueue(im, [sc, sr, col, th](ImDrawList* dl) {
        dl->AddCircle(sc, sr, col, 0, th);
    });
}

void DrawContext::DrawCircleFilled(Widgets::Vec2 center, float radius, Widgets::Vec4 color,
                                   uint32_t hitTarget) {
    auto& im    = *_impl;
    ImVec2 sc   = ToScreen(im, center);
    float  sr   = ScaleLen(im, radius);
    ImU32  col  = ToImU32(color);
    int    lay  = im.layerStack.back();

    MaybeAddHit(im, lay, hitTarget, {sc.x - sr, sc.y - sr}, {sc.x + sr, sc.y + sr});

    Enqueue(im, [sc, sr, col](ImDrawList* dl) {
        dl->AddCircleFilled(sc, sr, col);
    });
}

void DrawContext::DrawEllipse(Widgets::Vec2 center, Widgets::Vec2 radius, Widgets::Vec4 color,
                              float thickness, uint32_t hitTarget) {
    auto& im    = *_impl;
    ImVec2 sc   = ToScreen(im, center);
    float  srx  = ScaleLen(im, radius.x);
    float  sry  = ScaleLen(im, radius.y);
    ImU32  col  = ToImU32(color);
    float  th   = thickness;
    int    lay  = im.layerStack.back();

    MaybeAddHit(im, lay, hitTarget, {sc.x - srx, sc.y - sry}, {sc.x + srx, sc.y + sry});

    Enqueue(im, [sc, srx, sry, col, th](ImDrawList* dl) {
        dl->AddEllipse(sc, {srx, sry}, col, 0.f, 0, th);
    });
}

void DrawContext::DrawPolyline(const Widgets::Vec2* points, int count, Widgets::Vec4 color,
                               bool closed, float thickness, uint32_t hitTarget) {
    if (count < 2) return;

    auto& im = *_impl;
    ImU32 col = ToImU32(color);
    float th  = thickness;
    int   lay = im.layerStack.back();

    std::vector<ImVec2> screenPts;
    screenPts.reserve(static_cast<std::size_t>(count));
    float xMin = std::numeric_limits<float>::max();
    float yMin = std::numeric_limits<float>::max();
    float xMax = std::numeric_limits<float>::lowest();
    float yMax = std::numeric_limits<float>::lowest();

    for (int i = 0; i < count; ++i) {
        ImVec2 sp = ToScreen(im, points[i]);
        screenPts.push_back(sp);
        xMin = std::min(xMin, sp.x - th);
        yMin = std::min(yMin, sp.y - th);
        xMax = std::max(xMax, sp.x + th);
        yMax = std::max(yMax, sp.y + th);
    }

    MaybeAddHit(im, lay, hitTarget, {xMin, yMin}, {xMax, yMax});

    Enqueue(im, [screenPts = std::move(screenPts), col, closed, th](ImDrawList* dl) {
        dl->AddPolyline(screenPts.data(), static_cast<int>(screenPts.size()),
                        col, closed ? ImDrawFlags_Closed : 0, th);
    });
}

void DrawContext::DrawBezierCubic(Widgets::Vec2 p0, Widgets::Vec2 p1,
                                  Widgets::Vec2 p2, Widgets::Vec2 p3,
                                  Widgets::Vec4 color, float thickness, uint32_t hitTarget) {
    auto& im     = *_impl;
    ImVec2 sp0   = ToScreen(im, p0);
    ImVec2 scp0  = ToScreen(im, p1);
    ImVec2 scp1  = ToScreen(im, p2);
    ImVec2 sp3   = ToScreen(im, p3);
    ImU32  col   = ToImU32(color);
    float  th    = thickness;
    int    lay   = im.layerStack.back();

    const float xMin = std::min({sp0.x, scp0.x, scp1.x, sp3.x}) - th;
    const float yMin = std::min({sp0.y, scp0.y, scp1.y, sp3.y}) - th;
    const float xMax = std::max({sp0.x, scp0.x, scp1.x, sp3.x}) + th;
    const float yMax = std::max({sp0.y, scp0.y, scp1.y, sp3.y}) + th;
    MaybeAddHit(im, lay, hitTarget, {xMin, yMin}, {xMax, yMax});

    Enqueue(im, [sp0, scp0, scp1, sp3, col, th](ImDrawList* dl) {
        dl->AddBezierCubic(sp0, scp0, scp1, sp3, col, th);
    });
}

void DrawContext::DrawText(Widgets::Vec2 pos, Widgets::Vec4 color, const char* text,
                           float fontSize) {
    if (!text || *text == '\0') return;
    auto& im   = *_impl;
    ImVec2 sp  = ToScreen(im, pos);
    ImU32  col = ToImU32(color);
    float  sz  = (fontSize > 0.f) ? ScaleLen(im, fontSize) : 0.f;

    std::string str{text};
    Enqueue(im, [sp, col, str = std::move(str), sz](ImDrawList* dl) {
        if (sz > 0.f) {
            dl->AddText(nullptr, sz, sp, col, str.c_str());
        } else {
            dl->AddText(sp, col, str.c_str());
        }
    });
}

void DrawContext::DrawImage(Widgets::TextureHandle textureId,
                            Widgets::Vec2 min, Widgets::Vec2 max,
                            Widgets::Vec2 uv0, Widgets::Vec2 uv1,
                            Widgets::Vec4 tint) {
    auto& im    = *_impl;
    ImVec2 sMin = ToScreen(im, min);
    ImVec2 sMax = ToScreen(im, max);
    ImU32  col  = ToImU32(tint);

    Enqueue(im, [sMin, sMax, textureId, uv0, uv1, col](ImDrawList* dl) {
        dl->AddImage(ImTextureRef{textureId},
                     sMin, sMax,
                     {uv0.x, uv0.y}, {uv1.x, uv1.y},
                     col);
    });
}

// ─── Layer stack ──────────────────────────────────────────────────────────────

void DrawContext::PushLayer(int layerIndex) {
    _impl->layerStack.push_back(layerIndex);
}

void DrawContext::PopLayer() {
    if (_impl->layerStack.size() > 1) {
        _impl->layerStack.pop_back();
    }
}

// ─── Transform stack ──────────────────────────────────────────────────────────

void DrawContext::PushTransform(Transform2D t) {
    constexpr std::size_t MAX_DEPTH = 32;
    if (_impl->transformStack.size() >= MAX_DEPTH) return;
    // new = current.Compose(t): apply t first, then existing accumulated transform
    _impl->transformStack.push_back(_impl->transformStack.back().Compose(t));
}

void DrawContext::PopTransform() {
    if (_impl->transformStack.size() > 1) {
        _impl->transformStack.pop_back();
    }
}

// ─── Static subtree caching ───────────────────────────────────────────────────

bool DrawContext::BeginStatic(uint64_t cacheKey) {
    auto& im = *_impl;
    im.inStaticBlock   = true;
    im.currentCacheKey = cacheKey;

    auto it = im.staticCache->find(cacheKey);
    if (it != im.staticCache->end()) {
        for (auto& cmd : it->second.commands) {
            im.deferred->push_back(cmd);
        }
        im.staticCacheHit = true;
        return false; // caller may skip re-issuing
    }

    im.staticCacheHit    = false;
    im.staticBlockStart  = im.deferred->size();
    return true;
}

void DrawContext::EndStatic() {
    auto& im = *_impl;
    if (!im.inStaticBlock) return;

    if (!im.staticCacheHit) {
        auto& entry = (*im.staticCache)[im.currentCacheKey];
        entry.commands.assign(im.deferred->begin() + static_cast<std::ptrdiff_t>(im.staticBlockStart),
                              im.deferred->end());
    }

    im.inStaticBlock = false;
}

} // namespace ImFrame::Rendering
