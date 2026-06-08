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

#include <imgui.h>

namespace ImFrame::Overlay {

// ─── PopupScope ───────────────────────────────────────────────────────────────

PopupScope::~PopupScope() {
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

} // namespace ImFrame::Overlay
