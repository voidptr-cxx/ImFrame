/**
 * @file     DockSpaceWidget.hpp
 * @brief    Declarative, opt-in dockspace — a `Tree::PrimitiveWidget` usable inside a `Build()` tree
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-09
 * @version  2.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"

namespace ImFrame::App {

/**
 * @class    DockSpaceWidget
 * @brief    Declarative dockspace primitive wrapping the same ImGui calls `App::DockSpace` uses
 *
 * Unlike `App::DockSpace` (stateful layout-persistence infrastructure, owned
 * and driven automatically once per frame by `Application::RunOneFrame()`),
 * `DockSpaceWidget` is a plain, stateless `Tree::PrimitiveWidget`: place it
 * inside your own `Build()` tree (typically via `Application::SetRoot()`) to
 * register a dockspace host window and dock node without going through
 * `Application`'s automatic one. It has no named-layout persistence
 * (`SaveLayout`/`LoadLayout`/`ResetLayout`) — use `App::DockSpace` via
 * `Application::GetDockSpace()` for that.
 *
 * @note     No child/content slot in this phase. `ImGui::DockSpace()`
 *           consumes its entire host window's content region to register the
 *           dock node — anything painted directly afterward in that same
 *           window is clipped away (`RootBridge.hpp` documents the identical
 *           problem for the tree's own root window). Properly hosting a
 *           child here needs its own dedicated, uniquely-identified inner
 *           window (like `RootBridge`'s), which is real design work beyond
 *           this widget's current scope — see `DECISIONS.md` (Phase 30.3).
 *           Pair this widget with separately opened dockable windows
 *           (raw `ImGui::Begin()` calls, or another element's own window)
 *           that dock into it, exactly like `App::DockSpace` works today.
 *
 * Like `App::DockSpace`, this always covers the full main viewport regardless
 * of where it appears structurally in the tree — a dockspace host window is
 * inherently a top-level, viewport-filling construct, not a nested one.
 *
 * @since    2.6.0
 *
 * @example
 * @code
 * struct AppRoot {
 *     [[nodiscard]] Tree::Widget Build() const { return App::DockSpaceWidget(); }
 * };
 * AppRoot root;
 * app.SetRoot(root); // registers the dockspace; open separate dockable windows elsewhere
 * @endcode
 */
class DockSpaceWidget {
public:
    DockSpaceWidget() = default;

    /// Reserve a menu bar area at the top of the dockspace window (populate it via `ImGui::BeginMenuBar()` elsewhere in the frame).
    DockSpaceWidget& MenuBar(bool enabled = true) noexcept { _menuBar = enabled; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    DockSpaceWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] bool GetMenuBar() const noexcept { return _menuBar; }

    /// @internal Produces this dockspace's concrete `Element`. Defined in `src/Tree/RenderObjects/DockSpaceWidgetRO.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    bool      _menuBar = false;
    Tree::Key _key;
};

} // namespace ImFrame::App
