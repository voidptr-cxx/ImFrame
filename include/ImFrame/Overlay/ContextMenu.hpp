/**
 * @file     ContextMenu.hpp
 * @brief    Right-click context menu wrapping ImGui popup context helpers
 *
 * `ContextMenu` is a persistent fluent builder: add items with `Item()` and
 * `Separator()`, then call `Show()` (right-click on last widget) or
 * `ShowWindow()` (right-click anywhere in the current window) once per frame.
 * The item list is persistent — you do not need to rebuild it every frame.
 *
 * Each `Item` carries a `std::function<void()>` action that fires when the
 * menu entry is clicked.  Disabled items are greyed out and non-interactive.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ImFrame::Overlay {

// ─── ContextMenu ──────────────────────────────────────────────────────────────

/**
 * @class    ContextMenu
 * @brief    Persistent right-click context menu with a fluent item builder
 *
 * Build the menu once (or rebuild each frame if items are dynamic), then call
 * `Show()` or `ShowWindow()` to make it activatable.
 *
 * @since    1.3.0
 *
 * @example
 * @code
 * ContextMenu fileMenu("##file_ctx");
 * fileMenu.Item("New",    [](){ createFile(); })
 *         .Item("Open",   [](){ openFile(); })
 *         .Separator()
 *         .Item("Delete", [](){ deleteFile(); }, canDelete);
 *
 * // In UI code each frame:
 * ImGui::Selectable("file.txt");
 * fileMenu.Show();  // right-click on "file.txt" to activate
 * @endcode
 */
class [[deprecated("Use ContextMenuWidget instead. See Docs/Migration_v1_to_v2.md.")]] ContextMenu {
public:
    /**
     * @brief    Construct with a unique ImGui popup identifier
     * @param[in]  id  Popup ID string (should start with ## to hide from title bar)
     * @throws   Nothing
     */
    explicit ContextMenu(std::string id = "##ctx_menu");

    /**
     * @brief    Add a clickable menu item
     *
     * @param[in]  label    Display text
     * @param[in]  action   Callback invoked when the item is clicked
     * @param[in]  enabled  When `false` the item is greyed out and non-interactive
     * @return   Reference to this ContextMenu for chaining
     */
    ContextMenu& Item(std::string label, std::function<void()> action, bool enabled = true);

    /**
     * @brief    Add a horizontal separator between items
     * @return   Reference to this ContextMenu for chaining
     */
    ContextMenu& Separator();

    /**
     * @brief    Open on right-click of the last ImGui widget (BeginPopupContextItem)
     *
     * Must be called immediately after the widget whose right-click opens the menu.
     */
    void Show();

    /**
     * @brief    Open on right-click anywhere in the current ImGui window (BeginPopupContextWindow)
     */
    void ShowWindow();

private:
    struct Entry {
        std::string            label;
        std::function<void()>  action;
        bool                   enabled;
        bool                   isSeparator;
    };

    void RenderItems();

    std::string        _id;
    std::vector<Entry> _items;
};

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
 * @brief    Declarative right-click context menu — `Tree::PrimitiveWidget` replacement for `ContextMenu`
 *
 * Wraps a trigger child `Widget`; right-clicking it opens the menu via the
 * same `ImGui::BeginPopupContextItem()` used by `ContextMenu::Show()` — native
 * ImGui popups already escape parent clipping, so `Portal` is not needed here.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * ContextMenuWidget(Widget(Text("file.txt")))
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
