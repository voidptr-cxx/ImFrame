/**
 * @file     Modal.cpp
 * @brief    ModalElement — `ModalWidget`'s concrete `Element`
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  2.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Overlay/Modal.hpp"
#include "../Tree/ElementInternal.hpp"

#include <imgui.h>

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
