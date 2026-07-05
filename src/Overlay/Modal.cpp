/**
 * @file     Modal.cpp
 * @brief    Modal and ConfirmModal ImGui popup implementations
 *
 * @internal
 * `PopupScope::~PopupScope()` calls `ImGui::EndPopupModal()` **only** when
 * both `_open` (BeginPopupModal returned true) and `_active` (not moved-from)
 * are set.  This differs from ChildScope where EndChild is always called.
 *
 * `Modal::Begin()` handles the deferred-open pattern: if `_pendingOpen` is
 * set it calls `ImGui::OpenPopup()` before `BeginPopupModal()` so both happen
 * within the same frame.
 *
 * `ConfirmModal::Show()` is fully self-contained: it opens, renders, fires the
 * callback, and closes in a single call site.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Overlay/Modal.hpp"
#include "../Tree/ElementInternal.hpp"

#include <imgui.h>

namespace ImFrame::Overlay {

// MSVC's C4996 fires on the deprecated `Modal`/`ConfirmModal`'s own out-of-line
// fluent setters below (their own-class return type counts as a "use" of the
// deprecated class, even in the class's own implementation) — suppressed here
// since this is the deprecated API's own continued implementation, not an
// external caller.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

// ─── PopupScope ───────────────────────────────────────────────────────────────

PopupScope::~PopupScope() noexcept {
    if (_active && _open) {
        ImGui::EndPopup();
    }
}

PopupScope::PopupScope(PopupScope&& other) noexcept
    : _open(other._open)
    , _active(other._active)
{
    other._active = false;
}

// ─── Modal ────────────────────────────────────────────────────────────────────

Modal::Modal(std::string title) : _title(std::move(title)) {}

Modal& Modal::Size(float w, float h) {
    _w = w;
    _h = h;
    return *this;
}

Modal& Modal::NoClose(bool noClose) {
    _noClose = noClose;
    return *this;
}

void Modal::Open() {
    _pendingOpen = true;
}

PopupScope Modal::Begin() {
    if (_pendingOpen) {
        ImGui::OpenPopup(_title.c_str());
        _pendingOpen = false;
    }

    if (_w > 0.0f || _h > 0.0f) {
        ImGui::SetNextWindowSize({ _w, _h }, ImGuiCond_Always);
    }

    // Pass a dummy bool when the × button is desired. ImGui sets it to false on click;
    // the popup then closes naturally on the next frame.  When _noClose, pass nullptr.
    bool showClose = !_noClose;
    bool dummy     = true;
    const bool open = ImGui::BeginPopupModal(
        _title.c_str(),
        showClose ? &dummy : nullptr,
        ImGuiWindowFlags_None
    );

    return PopupScope(open);
}

void Modal::Close() {
    ImGui::CloseCurrentPopup();
}

// ─── ConfirmModal ─────────────────────────────────────────────────────────────

ConfirmModal::ConfirmModal(std::string id) : _id(std::move(id)) {
    _title = _id;
}

ConfirmModal& ConfirmModal::Title(std::string title) {
    _title = std::move(title);
    return *this;
}

ConfirmModal& ConfirmModal::Message(std::string message) {
    _message = std::move(message);
    return *this;
}

ConfirmModal& ConfirmModal::ConfirmLabel(std::string label) {
    _confirmLabel = std::move(label);
    return *this;
}

ConfirmModal& ConfirmModal::CancelLabel(std::string label) {
    _cancelLabel = std::move(label);
    return *this;
}

ConfirmModal& ConfirmModal::OnResult(Utility::Delegate<void(bool)> callback) {
    _onResult = std::move(callback);
    return *this;
}

void ConfirmModal::Open() {
    _pendingOpen = true;
}

void ConfirmModal::Show() {
    // Build a popup name: title##id avoids conflicts when title is shared
    const std::string popupName = _title + "##" + _id;

    if (_pendingOpen) {
        ImGui::OpenPopup(popupName.c_str());
        _pendingOpen = false;
    }

    ImGui::SetNextWindowSize({ 360.0f, 0.0f }, ImGuiCond_Always);

    if (ImGui::BeginPopupModal(popupName.c_str(), nullptr,
                               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        if (!_message.empty()) {
            ImGui::TextWrapped("%s", _message.c_str());
            ImGui::Spacing();
        }

        const float buttonW = 110.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float totalW  = buttonW * 2.0f + spacing;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - totalW) * 0.5f +
                             ImGui::GetCursorPosX());

        if (ImGui::Button(_confirmLabel.c_str(), { buttonW, 0.0f })) {
            if (_onResult) _onResult(true);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button(_cancelLabel.c_str(), { buttonW, 0.0f })) {
            if (_onResult) _onResult(false);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace ImFrame::Overlay

// ─── ModalWidget / ModalElement (Phase 29) ──────────────────────────────────────

namespace ImFrame::Internal {

class ModalElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        Sync(widget);
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        Sync(newWidget);
    }

    void Unmount() override {
        if (_child) { _child->Unmount(); }
        _child.reset();
    }

    /// Occupies no space at its structural position — renders via ImGui's independent popup layer.
    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints /*constraints*/) override {
        _size = {0.0f, 0.0f};
        return _size;
    }

    void Paint(Widgets::Vec2 /*position*/) override {
        bool* openPtr = _config.GetOpen();
        const bool wantsOpen = openPtr && *openPtr;

        if (wantsOpen && !_wasOpen) {
            ImGui::OpenPopup(_config.GetTitle().c_str());
        }
        _wasOpen = wantsOpen;

        if (_config.GetWidth() > 0.0f || _config.GetHeight() > 0.0f) {
            ImGui::SetNextWindowSize(ImVec2{_config.GetWidth(), _config.GetHeight()}, ImGuiCond_Always);
        }

        bool       showCloseDummy = true;
        bool*      closeFlag      = _config.GetNoClose() ? nullptr : &showCloseDummy;
        const bool isOpen = ImGui::BeginPopupModal(_config.GetTitle().c_str(), closeFlag, ImGuiWindowFlags_None);

        if (isOpen) {
            if (_child) {
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                (void)_child->Layout(Tree::BoxConstraints::Loose({avail.x, avail.y}));
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                _child->Paint({pos.x, pos.y});
            }
            if (closeFlag && !showCloseDummy && openPtr) {
                *openPtr = false; // × button clicked
            }
            ImGui::EndPopup();
        } else if (wantsOpen && openPtr) {
            // Closed via Escape or click-outside without going through the × button.
            *openPtr = false;
        }
    }

private:
    void Sync(const Tree::Widget& widget) {
        _config = widget.As<Overlay::ModalWidget>();
        const Tree::Widget* content = _config.GetContent() ? &*_config.GetContent() : nullptr;
        ReconcileChild(this, _child, content);
    }

    Overlay::ModalWidget            _config{"", nullptr};
    std::unique_ptr<Tree::Element> _child;
    bool                            _wasOpen = false;
};

} // namespace ImFrame::Internal

namespace ImFrame::Overlay {

std::unique_ptr<Tree::Element> ModalWidget::CreateElement() const {
    return std::make_unique<Internal::ModalElement>();
}

} // namespace ImFrame::Overlay
