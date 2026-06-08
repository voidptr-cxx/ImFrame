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

#include <functional>
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
class ContextMenu {
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

} // namespace ImFrame::Overlay
