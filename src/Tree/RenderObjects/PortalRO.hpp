/**
 * @file     PortalRO.hpp
 * @brief    `Portal` primitive's concrete `Element` — deferred root-scope rendering
 *
 * Defined in a header (unlike most `RenderObjects/*.cpp`-local `Element`
 * subclasses) because `Reconciler.cpp` needs to call `RenderDeferred()` on each
 * drained portal after the main tree paints.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Portal.hpp"
#include "../ElementInternal.hpp"
#include "../PortalRegistry.hpp"

#include <memory>

namespace ImFrame::Internal {

/**
 * @class    PortalElement
 * @brief    Owns a `Portal`'s child `Element`; defers its Layout/Paint to root scope
 *
 * At its own structural position, `Layout()` returns zero size and `Paint()` is
 * a no-op — the portal occupies no space and draws nothing where it is declared.
 * `RenderDeferred()`, called by `Reconciler::Show()` after the main tree paints,
 * performs the child's real `Layout()`/`Paint()` at root scope.
 *
 * @since    2.4.0
 */
class PortalElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        Sync(widget);
        RegisterPortal(this);
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        Sync(newWidget);
        RegisterPortal(this);
    }

    void Unmount() override {
        UnregisterPortal(this);
        if (_child) { _child->Unmount(); }
        _child.reset();
    }

    /// Occupies no space at its structural position — the child is laid out at root scope instead.
    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints /*constraints*/) override {
        _size = {0.0f, 0.0f};
        return _size;
    }

    /// Draws nothing at its structural position — see `RenderDeferred()`.
    void Paint(Widgets::Vec2 /*position*/) override {}

    /**
     * @brief    Lays out and paints this portal's child at root scope.
     *
     * Called by `Reconciler::Show()` once per frame, after the main tree has
     * painted, for every portal drained from the registry.
     *
     * @param[in] availableSize  Root window's available content size.
     * @param[in] position       Root window's cursor screen position.
     */
    void RenderDeferred(Widgets::Vec2 availableSize, Widgets::Vec2 position) {
        if (!_child) { return; }
        (void)_child->Layout(Tree::BoxConstraints::Loose(availableSize));
        _child->Paint(position);
    }

private:
    void Sync(const Tree::Widget& widget) {
        const auto&         portal      = widget.As<Tree::Portal>();
        const Tree::Widget* childWidget = &portal.GetChild();
        ReconcileChild(this, _child, childWidget);
    }

    std::unique_ptr<Tree::Element> _child;
};

} // namespace ImFrame::Internal
