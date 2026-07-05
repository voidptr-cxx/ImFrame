/**
 * @file     Portal.hpp
 * @brief    Renders a child widget at the element-tree root, escaping parent clipping
 *
 * `Portal` lets a deeply-nested widget (e.g. a `Combo` popup, a `Modal` backdrop,
 * a `ToastManager` stack) paint as if it were declared at the root of the tree,
 * regardless of where it structurally sits. This is how overlays draw on top of
 * everything else without being clipped by an ancestor's bounds.
 *
 * **Mechanism**
 * `Internal::PortalElement` occupies zero space and paints nothing at its own
 * structural position — instead, during `Mount()`/`Update()` it registers itself
 * with the thread-local portal registry (`src/Tree/PortalRegistry.hpp`). After
 * `Reconciler::Show()` finishes laying out and painting the main tree, it drains
 * the registry and calls `RenderDeferred()` on each portal in registration order,
 * so portal content is always the last thing drawn each frame (renders on top).
 *
 * **Lifetime**
 * A `Portal`'s child `Element` is owned by `Internal::PortalElement`, which lives
 * at the `Portal`'s normal structural position in the tree. When an ancestor
 * unmounts, ordinary tree teardown unmounts the `PortalElement` too, which
 * unregisters from the portal registry and unmounts its child — no special-case
 * cleanup is needed.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"

namespace ImFrame::Tree {

/**
 * @class    Portal
 * @brief    Renders `child` at the root of the element tree, on top of all non-portal content
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * struct MyOverlay {
 *     Widget Build() const {
 *         return Portal(Box().Background({0,0,0,0.5f}).Child(Text("On top!")));
 *     }
 * };
 * @endcode
 */
class Portal {
public:
    /// Constructs a Portal wrapping `child`, which renders at root scope.
    explicit Portal(Widget child) : _child(std::move(child)) {}

    /// Explicit identity override — see `Tree::Key`.
    Portal& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key    GetKey() const noexcept { return _key; }
    [[nodiscard]] const Widget& GetChild() const noexcept { return _child; }

    /// @internal Produces this portal's concrete `Element`. Defined in `PortalRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    Widget    _child;
    Tree::Key _key;
};

} // namespace ImFrame::Tree
