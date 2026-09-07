/**
 * @file     Modal.hpp
 * @brief    Declarative blocking modal dialog
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  2.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"

#include <memory>
#include <optional>
#include <string>

namespace ImFrame::Overlay {

// ─── ModalWidget (Phase 29) ─────────────────────────────────────────────────────

/**
 * @class    ModalWidget
 * @brief    Declarative modal dialog — `Tree::PrimitiveWidget` wrapper around `ImGui::BeginPopupModal()`
 *
 * Binds to a caller-owned `bool* open`: set `*open = true` to trigger opening
 * (e.g. from a `ButtonWidget::OnClick`), and the widget clears it back to
 * `false` on close (× button, Escape, or `Close()`-equivalent user action).
 * Wraps `ImGui::BeginPopupModal()`/`ImGui::EndPopup()` directly — native ImGui
 * modal popups already escape parent clipping on their own overlay layer, so
 * `Portal` is not needed here.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * bool showSettings = false;
 * // ...
 * ButtonWidget("Settings").OnClick([&] { showSettings = true; });
 * ModalWidget("Settings", &showSettings).Content(Text("Settings content"));
 * @endcode
 */
class ModalWidget {
public:
    /// `open` must outlive this widget and every `Element` mounted from it.
    ModalWidget(std::string title, bool* open) : _title(std::move(title)), _open(open) {}

    ModalWidget& Size(float w, float h) noexcept { _w = w; _h = h; return *this; }
    ModalWidget& NoClose(bool noClose = true) noexcept { _noClose = noClose; return *this; }
    ModalWidget& Content(Tree::Widget content) { _content.emplace(std::move(content)); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    ModalWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetTitle() const noexcept { return _title; }
    [[nodiscard]] bool* GetOpen() const noexcept { return _open; }
    [[nodiscard]] float GetWidth() const noexcept { return _w; }
    [[nodiscard]] float GetHeight() const noexcept { return _h; }
    [[nodiscard]] bool GetNoClose() const noexcept { return _noClose; }
    [[nodiscard]] const std::optional<Tree::Widget>& GetContent() const noexcept { return _content; }

    /// @internal Produces this modal's concrete `Element`. Defined in `Modal.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    std::string                  _title;
    bool*                        _open = nullptr;
    float                        _w = 0.0f;
    float                        _h = 0.0f;
    bool                         _noClose = false;
    std::optional<Tree::Widget> _content;
    Tree::Key                    _key;
};

} // namespace ImFrame::Overlay
