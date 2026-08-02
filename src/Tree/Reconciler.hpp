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
#include "../Rendering/Renderers/ImGuiCompatRenderer.hpp"

#include <memory>

namespace ImFrame::Internal {

class Reconciler {
public:
    /**
     * @brief    Reconciles, lays out, and paints `rootWidget` for the current frame.
     * @param[in] rootWidget  Freshly built root widget (typically `someComponent.Build()`).
     */
    void Show(const Tree::Widget& rootWidget);

    /**
     * @brief    Swaps the renderer used to replay each frame's `CommandBuffer`.
     * @param[in] renderer  Must not be null. Defaults to an `ImGuiCompatRenderer`.
     *
     * @internal
     * `Internal`-only — there is no public `Application::UseNativeRenderer()` yet.
     * Exposing renderer selection publicly needs a public (non-`Internal::`) handle
     * type (`Application.hpp` may never name `Internal::IRenderer` directly — see
     * `.claude/CLAUDE.md`'s "never expose `Internal::` in public headers" invariant),
     * which is real, undesigned API surface, not attempted in Phase 32.5. See
     * `.claude/DECISIONS.md`.
     */
    void SetRenderer(std::unique_ptr<IRenderer> renderer);

private:
    std::unique_ptr<Tree::Element> _rootElement;

    /// Reused frame-to-frame (see `Rendering::CommandBuffer`'s own file comment on why this
    /// is a single reused buffer, not a literal pool, in this single-threaded render loop).
    Rendering::CommandBuffer _commandBuffer;

    /// Defaults to `ImGuiCompatRenderer` — promoted from a fixed member to a swappable
    /// pointer in Phase 32.5, now that a second real `IRenderer` implementation
    /// (`NativeRendererGL3`) exists to select between (see Phase 31.3's `DECISIONS.md`
    /// row, which named this exact promotion point).
    std::unique_ptr<IRenderer> _renderer = std::make_unique<ImGuiCompatRenderer>();
};

} // namespace ImFrame::Internal
