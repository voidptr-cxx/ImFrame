/**
 * @file     Modal.hpp
 * @brief    Blocking modal dialog and pre-built confirm/cancel variant
 *
 * `Modal` wraps `ImGui::BeginPopupModal` / `ImGui::EndPopupModal` behind a
 * `PopupScope` RAII guard.  `Open()` uses a `_pendingOpen` flag so the caller
 * never touches `ImGui::OpenPopup()` directly — the deferred call happens
 * inside `Begin()` on the same frame.
 *
 * `ConfirmModal` is a self-contained two-button dialog (Confirm + Cancel) built
 * on top of `Modal`.  Call `Open()` to trigger it and `Show()` every frame;
 * the `OnResult` delegate fires once when the user clicks either button.
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

#include <memory>
#include <optional>
#include <string>

namespace ImFrame::Overlay {

// ─── PopupScope ───────────────────────────────────────────────────────────────

/**
 * @class    PopupScope
 * @brief    RAII guard that calls `ImGui::EndPopupModal()` when a modal is open
 *
 * Returned by `Modal::Begin()`.  Use as an `if` condition: the body runs only
 * when the modal is actually open.  The destructor calls `EndPopupModal()`
 * **only** if the popup was open (`operator bool()` is `true`) — this differs
 * from `ChildScope` where `EndChild()` is always called.
 *
 * Moveable, non-copyable.  The moved-from scope is marked inactive.
 *
 * @since    1.3.0
 *
 * @example
 * @code
 * if (auto scope = myModal.Begin()) {
 *     ImGui::Text("Modal content");
 *     if (ImGui::Button("Close")) myModal.Close();
 * }
 * @endcode
 */
class PopupScope {
public:
    /**
     * @brief    Construct from the return value of `ImGui::BeginPopupModal()`
     * @param[in]  open  `true` if the modal is currently open
     * @throws   Nothing — noexcept
     */
    explicit PopupScope(bool open) noexcept : _open(open) {}

    /// Calls `ImGui::EndPopupModal()` if this scope is active and the popup was open
    ~PopupScope() noexcept;

    PopupScope(const PopupScope&)            = delete;
    PopupScope& operator=(const PopupScope&) = delete;

    /** @brief   Transfer ownership; the moved-from scope becomes inactive */
    PopupScope(PopupScope&& other) noexcept;
    PopupScope& operator=(PopupScope&&) = delete;

    /**
     * @brief    Convert to `bool` for use in `if` conditions
     * @return   `true` if the modal is open and content should be rendered
     */
    [[nodiscard]] explicit operator bool() const noexcept { return _open; }

private:
    bool _open   = false; ///< Whether `BeginPopupModal()` returned true
    bool _active = true;  ///< `false` after move — prevents double EndPopupModal
};

// ─── Modal ────────────────────────────────────────────────────────────────────

/**
 * @class    Modal
 * @brief    Fluent builder for a titled blocking modal dialog
 *
 * Use `Open()` to trigger opening (sets a deferred flag) and call `Begin()`
 * every frame.  `Begin()` handles the `ImGui::OpenPopup()` call internally on
 * the frame when the flag is set, ensuring correct ImGui deferred-popup
 * semantics.
 *
 * @note     Non-copyable, non-moveable — owns per-window state.
 *
 * @since    1.3.0
 *
 * @example
 * @code
 * Modal settingsModal("Settings##settings");
 * settingsModal.Size(400.0f, 300.0f).NoClose(true);
 *
 * // In UI code:
 * if (ImGui::Button("Open Settings")) settingsModal.Open();
 * if (auto scope = settingsModal.Begin()) {
 *     ImGui::Text("Settings content");
 *     if (ImGui::Button("Done")) settingsModal.Close();
 * }
 * @endcode
 */
class [[deprecated("Use ModalWidget instead. See Docs/Migration_v1_to_v2.md.")]] Modal {
public:
    /**
     * @brief    Construct a modal with an ImGui popup identifier
     * @param[in]  title  Window title shown in the title bar (may include ##id suffix)
     * @throws   Nothing
     */
    explicit Modal(std::string title);

    /**
     * @brief    Set the fixed modal size (0 = auto-size on that axis)
     * @param[in]  w  Width in pixels
     * @param[in]  h  Height in pixels
     * @return   Reference to this Modal for chaining
     */
    Modal& Size(float w, float h);

    /**
     * @brief    Hide the close button on the title bar
     * @param[in]  noClose  `true` to remove the × button (default `true`)
     * @return   Reference to this Modal for chaining
     */
    Modal& NoClose(bool noClose = true);

    /**
     * @brief    Schedule the modal to open on the next `Begin()` call
     *
     * Safe to call from any ImGui context — the actual `OpenPopup` is deferred
     * into `Begin()`.
     */
    void Open();

    /**
     * @brief    Begin the modal frame, opening it if `Open()` was called
     *
     * Call every frame.  Returns a `PopupScope` that evaluates to `true` when
     * the popup is open; `EndPopupModal()` is called on scope destruction.
     *
     * @return   `[[nodiscard]]` PopupScope — use as an `if` condition
     */
    [[nodiscard]] PopupScope Begin();

    /**
     * @brief    Close the modal from within the popup body
     *
     * Calls `ImGui::CloseCurrentPopup()`. Must be called while the corresponding
     * `PopupScope` is alive (i.e. inside the `if (scope)` body).
     */
    void Close();

    Modal(const Modal&)            = delete;
    Modal& operator=(const Modal&) = delete;
    Modal(Modal&&)                 = delete;
    Modal& operator=(Modal&&)      = delete;

private:
    std::string _title;
    float       _w           = 0.0f;
    float       _h           = 0.0f;
    bool        _noClose     = false;
    bool        _pendingOpen = false;
};

// ─── ConfirmModal ─────────────────────────────────────────────────────────────

/**
 * @class    ConfirmModal
 * @brief    Pre-built two-button confirm / cancel dialog
 *
 * Self-contained: `Show()` handles open, render, and result dispatch.  Call
 * `Open()` to trigger it and `Show()` every frame.  The `OnResult` delegate
 * fires exactly once with `true` for confirm and `false` for cancel.
 *
 * @note     Non-copyable, non-moveable — owns modal state.
 *
 * @since    1.3.0
 *
 * @example
 * @code
 * ConfirmModal deleteConfirm("##delete");
 * deleteConfirm.Title("Delete File")
 *              .Message("This action cannot be undone.")
 *              .OnResult([](bool confirmed) {
 *                  if (confirmed) doDelete();
 *              });
 *
 * // In UI code:
 * if (ImGui::Button("Delete")) deleteConfirm.Open();
 * deleteConfirm.Show();
 * @endcode
 */
class [[deprecated("Use ModalWidget instead. See Docs/Migration_v1_to_v2.md.")]] ConfirmModal {
public:
    /**
     * @brief    Construct with a unique ImGui identifier (used as popup name)
     * @param[in]  id  Unique string identifier — shown as title if `Title()` is not called
     * @throws   Nothing
     */
    explicit ConfirmModal(std::string id);

    /**
     * @brief    Set the title bar text
     * @param[in]  title  Display title
     * @return   Reference to this ConfirmModal for chaining
     */
    ConfirmModal& Title(std::string title);

    /**
     * @brief    Set the body message shown above the buttons
     * @param[in]  message  Description text
     * @return   Reference to this ConfirmModal for chaining
     */
    ConfirmModal& Message(std::string message);

    /**
     * @brief    Override the confirm button label (default: "OK")
     * @param[in]  label  Button text
     * @return   Reference to this ConfirmModal for chaining
     */
    ConfirmModal& ConfirmLabel(std::string label);

    /**
     * @brief    Override the cancel button label (default: "Cancel")
     * @param[in]  label  Button text
     * @return   Reference to this ConfirmModal for chaining
     */
    ConfirmModal& CancelLabel(std::string label);

    /**
     * @brief    Register the result callback
     *
     * Fired exactly once per Open/Show cycle with `true` (confirm) or
     * `false` (cancel).
     *
     * @param[in]  callback  Callable accepting `bool`
     * @return   Reference to this ConfirmModal for chaining
     */
    ConfirmModal& OnResult(Utility::Delegate<void(bool)> callback);

    /**
     * @brief    Schedule the dialog to open on the next `Show()` call
     */
    void Open();

    /**
     * @brief    Render the dialog; handles open, content, and result dispatch
     *
     * Call every frame.  No-op when not triggered.
     */
    void Show();

    ConfirmModal(const ConfirmModal&)            = delete;
    ConfirmModal& operator=(const ConfirmModal&) = delete;
    ConfirmModal(ConfirmModal&&)                 = delete;
    ConfirmModal& operator=(ConfirmModal&&)      = delete;

private:
    std::string                       _id;
    std::string                       _title;
    std::string                       _message;
    std::string                       _confirmLabel = "OK";
    std::string                       _cancelLabel  = "Cancel";
    Utility::Delegate<void(bool)>     _onResult;
    bool                              _pendingOpen  = false;
};

// ─── ModalWidget (Phase 29) ─────────────────────────────────────────────────────

/**
 * @class    ModalWidget
 * @brief    Declarative modal dialog — `Tree::PrimitiveWidget` replacement for `Modal`
 *
 * Binds to a caller-owned `bool* open`: set `*open = true` to trigger opening
 * (e.g. from a `ButtonWidget::OnClick`), and the widget clears it back to
 * `false` on close (× button, Escape, or `Close()`-equivalent user action).
 * Wraps the exact same `ImGui::BeginPopupModal()`/`EndPopupModal()` pair as
 * `Modal::Begin()` — native ImGui modal popups already escape parent clipping
 * on their own overlay layer, so `Portal` is not needed here.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * bool showSettings = false;
 * // ...
 * ButtonWidget("Settings").OnClick([&] { showSettings = true; });
 * ModalWidget("Settings", &showSettings).Content(Widget(Text("Settings content")));
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
