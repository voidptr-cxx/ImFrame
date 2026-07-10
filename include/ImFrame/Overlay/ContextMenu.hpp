/**
 * @file     ContextMenu.hpp
 * @brief    Declarative right-click context menu wrapping ImGui popup context helpers
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  2.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ImFrame::Overlay {

// ─── ContextMenuWidget (Phase 29) ───────────────────────────────────────────────

/**
 * @struct   ContextMenuEntry
 * @brief    One item (or separator) of a `ContextMenuWidget`
 * @since    2.4.0
 */
struct ContextMenuEntry {
    std::string                Label;
    Utility::Delegate<void()>  Action;
    bool                       Enabled     = true;
    bool                       IsSeparator = false;
};

/**
 * @class    ContextMenuWidget
 * @brief    Declarative right-click context menu — `Tree::PrimitiveWidget` wrapping `ImGui::BeginPopupContextItem()`
 *
 * Wraps a trigger child `Widget`; right-clicking it opens the menu via
 * `ImGui::BeginPopupContextItem()` — native ImGui popups already escape
 * parent clipping, so `Portal` is not needed here.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * ContextMenuWidget(Text("file.txt"))
 *     .Item("Open", [] { openFile(); })
 *     .Item("Delete", [] { deleteFile(); });
 * @endcode
 */
class ContextMenuWidget {
public:
    explicit ContextMenuWidget(Tree::Widget child) : _child(std::move(child)) {}

    ContextMenuWidget& Item(std::string label, Utility::Delegate<void()> action, bool enabled = true) {
        _items.push_back(ContextMenuEntry{std::move(label), std::move(action), enabled, false});
        return *this;
    }
    ContextMenuWidget& Separator() {
        _items.push_back(ContextMenuEntry{"", {}, true, true});
        return *this;
    }

    /// Explicit identity override — see `Tree::Key`.
    ContextMenuWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const Tree::Widget& GetChild() const noexcept { return _child; }
    [[nodiscard]] const std::vector<ContextMenuEntry>& GetItems() const noexcept { return _items; }

    /// @internal Produces this menu's concrete `Element`. Defined in `ContextMenu.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    Tree::Widget                    _child;
    std::vector<ContextMenuEntry>   _items;
    Tree::Key                       _key;
};

} // namespace ImFrame::Overlay
