/**
 * @file     Reconciler.cpp
 * @brief    Implementation of `Reconciler::Show()`
 *
 * @internal
 * Deliberately makes no raw ImGui calls — `BeginRootWindow()`/`EndRootWindow()`
 * are the only points of contact with ImGui, confined to
 * `RenderObjects/RootBridge.cpp` per the "RenderObject implementations live
 * exclusively in `src/Tree/RenderObjects/`" invariant.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Reconciler.hpp"
#include "PortalRegistry.hpp"
#include "RenderObjects/PortalRO.hpp"
#include "RenderObjects/RootBridge.hpp"

namespace ImFrame::Internal {

void Reconciler::Show(const Tree::Widget& rootWidget) {
    if (_rootElement && _rootElement->CanUpdate(rootWidget)) {
        _rootElement->Update(rootWidget);
    } else {
        if (_rootElement) { _rootElement->Unmount(); }
        _rootElement = rootWidget.CreateElement();
        _rootElement->Mount(nullptr, 0, rootWidget);
    }

    const RootWindowInfo info = BeginRootWindow();
    (void)_rootElement->Layout(Tree::BoxConstraints::Loose(info.AvailableSize));

    _commandBuffer.Reset();
    _rootElement->Paint(_commandBuffer, info.CursorScreenPos);

    // Portals registered themselves during the Mount/Update pass above; draining and
    // rendering them last (still inside the same root window) puts portal content
    // after all non-portal draw calls, so it always renders on top.
    for (PortalElement* portal : DrainPortals()) {
        portal->RenderDeferred(_commandBuffer, info.AvailableSize, info.CursorScreenPos);
    }

    // One replay for the whole frame's accumulated Box/Text commands, still
    // inside the root window (ImGuiCompatRenderer::Render() draws onto
    // ImGui::GetWindowDrawList()). Elements that open their own nested ImGui
    // window (e.g. VirtualListElement) flush their own local buffer before
    // closing that window instead of pushing into this one — see
    // VirtualListRO.cpp's Paint() comment.
    _renderer.Render(_commandBuffer);

    EndRootWindow();
}

} // namespace ImFrame::Internal
