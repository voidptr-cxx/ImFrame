/**
 * @file     ImGuiCompatRenderer.hpp
 * @brief    `IRenderer` implementation translating `CommandBuffer` into real ImGui draw-list calls
 *
 * @internal
 * This, alongside `src/Tree/RenderObjects/*.cpp`, is a second legitimate ImGui
 * call site — it exists specifically to be the one place that turns
 * `Rendering::Command` values into `ImDrawList` calls, per Phase 31's design.
 * See `.claude/DECISIONS.md`, Phase 31.2, for why `RenderObjects/` still also
 * calls ImGui directly for `GestureRegion`/`VirtualList`/`RootBridge`/
 * `DockSpace` (interaction/window state with no generic command equivalent) —
 * this file only ever handles what `BoxElement`/`TextElement` record.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-16
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "IRenderer.hpp"

#include <cstdint>
#include <unordered_map>

struct ImFont;

namespace ImFrame::Internal {

/**
 * @class    ImGuiCompatRenderer
 * @brief    Replays a `CommandBuffer` as ImGui draw-list calls on the current window's draw list
 *
 * @internal
 * Must be called while an ImGui window is current (i.e. between
 * `ImGui::Begin()`/`ImGui::End()`) — it draws onto `ImGui::GetWindowDrawList()`
 * at `Render()` time. `PushOpacityLayer`/`PushBlendLayer`/`PopLayer` currently
 * manage `ImDrawListSplitter` channel ordering only; true per-layer alpha/blend
 * compositing needs an offscreen render target ImGui's draw-list model doesn't
 * provide, so the `Opacity`/`BlendMode` values are accepted but not yet applied
 * to pixel output — no command producer needs this today (see Phase 31.2's
 * `DECISIONS.md` entry).
 *
 * `LoadFont()` (Phase 33.8) loads directly into ImGui's own `ImFontAtlas` — the
 * exact mechanism `Application::WithFont()` used inline before this sub-phase,
 * just relocated behind `IRenderer::LoadFont()`'s generic seam and now
 * recording a `Rendering::FontId -> ImFont*` mapping this renderer's own
 * `DrawText` translation consults (falling back to `ImGui::GetFont()`, the
 * pre-Phase-33.8 behaviour, for an invalid/unknown `FontId` — every existing
 * `DrawText` producer that never sets `Font` is therefore unaffected).
 */
class ImGuiCompatRenderer final : public IRenderer {
public:
    void Render(const Rendering::CommandBuffer& buffer) override;
    void Shutdown() override {}

    [[nodiscard]] Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) override;

private:
    [[nodiscard]] ImFont* ResolveFont(Rendering::FontId id) const;

    /// Keyed by FontId::Value() — same "no global std::hash<FontId>" reasoning as FontRegistry's
    /// own map (Phase 33.2).
    std::unordered_map<std::uint32_t, ImFont*> _fontsById;
    std::uint32_t                              _nextFontId = 1; ///< 0 stays FontId's reserved "invalid" value.
};

} // namespace ImFrame::Internal
