/**
 * @file     Reconciler.hpp
 * @brief    Drives Mount/Update, Layout, and Paint for the root of a widget tree
 *
 * Owns the single root `Element`. Each call to `Show()` reconciles a freshly
 * built root `Widget` against the existing root element (update in place if
 * the concrete type matches, otherwise destroy and recreate), then opens a
 * dedicated dockable host window (`RootBridge::BeginRootWindow()` —
 * `DockSpace::Begin()` already consumes the dockspace host window's own
 * content region to register the dock node, so the tree cannot paint directly
 * into it), runs one `Layout()` pass bounded by that window's content region,
 * and one `Paint()` pass at that window's cursor position.
 *
 * Phase 27 rebuilds the root every frame unconditionally — there is no
 * dirty-tracking yet. Phase 28's `State<T>::Set()` is expected to introduce
 * that on top of this same `Show()` entry point.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Element.hpp"
#include "ImFrame/Tree/Widget.hpp"

#include <memory>

namespace ImFrame::Internal {

class Reconciler {
public:
    /**
     * @brief    Reconciles, lays out, and paints `rootWidget` for the current frame.
     * @param[in] rootWidget  Freshly built root widget (typically `someComponent.Build()`).
     */
    void Show(const Tree::Widget& rootWidget);

private:
    std::unique_ptr<Tree::Element> _rootElement;
};

} // namespace ImFrame::Internal
