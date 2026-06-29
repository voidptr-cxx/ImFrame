/**
 * @file     ViewportRegistry.hpp
 * @brief    Per-frame Viewport state, framebuffer ownership, and dispatch machinery
 *
 * Internal to ImFrame core. Tracks all registered Viewport instances, drives the
 * DispatchViewportRenders / FlipShownFlags lifecycle, and owns each Viewport's
 * IViewportFramebuffer. Not part of the public API.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"
#include "ImFrame/Rendering/RenderContext.hpp"
#include "ImFrame/Rendering/Viewport.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ImFrame::Internal {

// ─── ViewportScreenRect ───────────────────────────────────────────────────────

/// Screen-space rectangle of a Viewport — updated by Show() each frame.
struct ViewportScreenRect {
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;

    [[nodiscard]] bool Contains(float px, float py) const noexcept {
        return px >= x && px < (x + width) && py >= y && py < (y + height);
    }
};

// ─── ViewportEntry ────────────────────────────────────────────────────────────

/// State for one registered Viewport, owned by ViewportRegistry.
struct ViewportEntry {
    std::string                         id;
    Rendering::Viewport*                viewport    = nullptr; ///< Borrowed pointer; not owned.
    std::uint32_t                       currentW    = 0;
    std::uint32_t                       currentH    = 0;
    bool                                wasShown    = false;   ///< Show() called last frame?
    bool                                shownNow    = false;   ///< Show() called this frame?
    ViewportScreenRect                  screenRect;
    std::uint64_t                       imTextureId = 0;       ///< Cached for Show()→ImGui::Image().
    std::unique_ptr<IViewportFramebuffer> framebuffer;
};

// ─── ViewportRegistry ─────────────────────────────────────────────────────────

/// Manages the set of registered Viewports for one Application.
class ViewportRegistry {
public:
    ViewportRegistry() = default;

    /// Register or retrieve the entry for a Viewport; create the framebuffer if needed.
    /// Called by Viewport::Show() on first appearance.
    ViewportEntry& GetOrCreate(Rendering::Viewport* vp,
                               std::uint32_t requestedW, std::uint32_t requestedH,
                               IBackend& backend);

    /// Remove the entry for the given ID. Called from ~Viewport() / UnregisterViewport().
    void Remove(std::string_view id);

    /// Render all Viewports that were shown last frame. Called before BeginFrame().
    void DispatchRenders(IBackend& backend,
                         float deltaTime, std::uint32_t frameIndex);

    /// After EndFrame(): promote shownNow → wasShown, clear shownNow. Update screen rects.
    void FlipShownFlags();

    /// Mark a Viewport as shown this frame and record its screen rectangle.
    /// Called by Viewport::Show() after ImGui::Image().
    void MarkShown(std::string_view id, const ViewportScreenRect& rect);

    /// Device-idle shutdown: destroy all framebuffers.
    void DestroyAll();

    [[nodiscard]] std::vector<ViewportEntry>& Entries() noexcept { return _entries; }
    [[nodiscard]] const std::vector<ViewportEntry>& Entries() const noexcept { return _entries; }

private:
    std::vector<ViewportEntry> _entries;
};

/// Build a ViewportImage from raw ViewportHandles + the active backend context.
[[nodiscard]] Rendering::ViewportImage MakeViewportImage(
    const NativeGraphicsContext& ctx, const ViewportHandles& h);

} // namespace ImFrame::Internal
