/**
 * @file     DrawContextImpl.hpp
 * @brief    Internal types shared between Canvas2D.cpp and DrawContext.cpp
 *
 * Not part of the public API. Both Canvas2D.cpp and DrawContext.cpp include
 * this header directly. Defines the ImGui-dependent implementation structs for
 * deferred draw commands, hit records, the static cache, and DrawContextImpl.
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

#pragma once

#include "ImFrame/Rendering/Camera2D.hpp"
#include "ImFrame/Rendering/Transform2D.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <imgui.h>

#include <functional>
#include <unordered_map>
#include <vector>

// ─── Per-frame types ──────────────────────────────────────────────────────────

/// Screen-space AABB registered for hit testing against a canvas element.
struct HitRecord {
    int      layerIndex;  ///< Layer this element was drawn on.
    uint32_t hitTargetId; ///< Caller-supplied hit-test ID.
    ImVec2   screenMin;   ///< Top-left of the AABB in screen pixels.
    ImVec2   screenMax;   ///< Bottom-right of the AABB in screen pixels.
};

/// A single deferred draw call destined for a specific layer.
struct DeferredDrawCmd {
    int                              layerIndex;
    std::function<void(ImDrawList*)> draw;
};

/// Cached draw commands for a static block keyed by cacheKey.
struct CachedBlock {
    std::vector<DeferredDrawCmd> commands;
};

// ─── DrawContextImpl ──────────────────────────────────────────────────────────

namespace ImFrame::Internal {

/**
 * @struct DrawContextImpl
 * @brief  Per-frame state for DrawContext — owned by Canvas2D::Show()
 * @internal
 */
struct DrawContextImpl {
    // Non-owning pointers into CanvasImpl storage (valid only during Show())
    std::vector<DeferredDrawCmd>*              deferred;
    std::vector<HitRecord>*                    hitRecords;
    std::unordered_map<uint64_t, CachedBlock>* staticCache;
    const Rendering::Camera2D*                 camera;

    ImVec2 viewportOrigin = {0.f, 0.f};
    ImVec2 viewportSize   = {0.f, 0.f};

    // Active layer and transform stacks
    std::vector<int>                   layerStack        = {0};
    std::vector<Rendering::Transform2D> transformStack   = {Rendering::Transform2D::Identity()};

    // BeginStatic / EndStatic state
    bool        inStaticBlock   = false;
    bool        staticCacheHit  = false;
    uint64_t    currentCacheKey = 0;
    std::size_t staticBlockStart = 0;
};

} // namespace ImFrame::Internal
