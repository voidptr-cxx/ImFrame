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
 */
class ImGuiCompatRenderer final : public IRenderer {
public:
    void Render(const Rendering::CommandBuffer& buffer) override;
    void Shutdown() override {}
};

} // namespace ImFrame::Internal
