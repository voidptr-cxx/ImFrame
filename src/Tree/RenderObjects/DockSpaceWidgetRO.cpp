/**
 * @file     DockSpaceWidgetRO.cpp
 * @brief    `DockSpaceWidget` primitive's concrete `Element`
 *
 * @internal
 * Like `PortalElement`, occupies zero space and ignores the `position` passed
 * to `Paint()` — a dockspace host window always covers the full main
 * viewport regardless of where it appears structurally in the tree (the
 * same reasoning `App::DockSpace::Begin()` already follows).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-09
 * @version  2.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/App/DockSpaceWidget.hpp"
#include "DockSpaceRO.hpp"

namespace ImFrame::Internal {

class DockSpaceWidgetElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<App::DockSpaceWidget>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<App::DockSpaceWidget>();
    }

    /// Occupies no space at its structural position — the dockspace window is pinned to the viewport regardless.
    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints /*constraints*/) override {
        _size = {0.0f, 0.0f};
        return _size;
    }

    /// Ignores `position` — opens/closes the dockspace host window at the main viewport's work area.
    void Paint(Widgets::Vec2 /*position*/) override {
        // Distinct window/dockspace-id names from App::DockSpace's "##DockSpace"/
        // "MainDockSpace" — Application::RunOneFrame() always drives the automatic
        // App::DockSpace too, so a SetRoot() tree using DockSpaceWidget runs both
        // in the same frame; reusing the same ImGui IDs would collide.
        const DockSpaceBeginInfo info =
            BeginDockSpaceWindow(_config.GetMenuBar(), "##ImFrameDockSpaceWidget", "ImFrameDockSpaceWidget");
        if (info.NeedsDefaultLayout) {
            // No layout persistence in this widget (documented limitation) — the
            // captured ini is only needed by App::DockSpace's ResetLayout().
            (void)InitDefaultLayoutNodes(info.Id);
        }
        EndDockSpaceWindow();
    }

private:
    App::DockSpaceWidget _config;
};

} // namespace ImFrame::Internal

namespace ImFrame::App {

std::unique_ptr<Tree::Element> DockSpaceWidget::CreateElement() const {
    return std::make_unique<Internal::DockSpaceWidgetElement>();
}

} // namespace ImFrame::App
