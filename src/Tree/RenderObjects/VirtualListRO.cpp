/**
 * @file     VirtualListRO.cpp
 * @brief    `VirtualList` primitive's concrete `Element` — visible-range virtualisation
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Tree/VirtualList.hpp"
#include "../ElementInternal.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>

namespace ImFrame::Internal {

using Tree::BoxConstraints;
using Tree::Element;
using Tree::VirtualList;
using Tree::Widget;

class VirtualListElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config.emplace(widget.As<VirtualList>());
    }

    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config.emplace(newWidget.As<VirtualList>());
    }

    void Unmount() override {
        for (auto& [index, elem] : _items) { elem->Unmount(); }
        _items.clear();
    }

    /// Fills whatever space the parent gives — the visible range is computed live in Paint() from ImGui's scroll state.
    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        _size = {constraints.MaxWidth, constraints.MaxHeight};
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        if (!_config) { return; }
        const int   itemCount  = _config->GetItemCount();
        const float itemHeight = _config->GetItemHeight();

        ImGui::SetCursorScreenPos(ImVec2{position.x, position.y});
        ImGui::PushID(this);
        ImGui::BeginChild("##vlist", ImVec2{_size.x, _size.y}, false);

        if (itemCount > 0 && itemHeight > 0.0f) {
            const float scrollY        = ImGui::GetScrollY();
            const float viewportHeight = ImGui::GetWindowHeight();

            int firstIdx = static_cast<int>(std::floor(scrollY / itemHeight));
            int lastIdx  = static_cast<int>(std::ceil((scrollY + viewportHeight) / itemHeight));
            firstIdx     = std::clamp(firstIdx, 0, itemCount - 1);
            lastIdx      = std::clamp(lastIdx, -1, itemCount - 1);

            // Unmount cached rows that scrolled out of the visible range.
            for (auto it = _items.begin(); it != _items.end();) {
                if (it->first < firstIdx || it->first > lastIdx) {
                    it->second->Unmount();
                    it = _items.erase(it);
                } else {
                    ++it;
                }
            }

            const BoxConstraints itemConstraints = BoxConstraints::Tight({_size.x, itemHeight});
            for (int i = firstIdx; i <= lastIdx; ++i) {
                Widget                    itemWidget = _config->GetBuilder()(i);
                std::unique_ptr<Element>& slot       = _items[i];
                if (slot && slot->CanUpdate(itemWidget)) {
                    slot->Update(itemWidget);
                } else {
                    if (slot) { slot->Unmount(); }
                    slot = itemWidget.CreateElement();
                    slot->Mount(this, static_cast<std::size_t>(i), itemWidget);
                }
                (void)slot->Layout(itemConstraints);
                ImGui::SetCursorPosY(static_cast<float>(i) * itemHeight);
                const ImVec2 rowPos = ImGui::GetCursorScreenPos();
                slot->Paint({rowPos.x, rowPos.y});
            }

            // Reserve the full (unvirtualized) scroll extent regardless of rows actually painted.
            ImGui::SetCursorPosY(static_cast<float>(itemCount) * itemHeight);
            ImGui::Dummy(ImVec2{0.0f, 0.0f});
        }

        ImGui::EndChild();
        ImGui::PopID();
    }

private:
    std::optional<VirtualList>                        _config;
    std::unordered_map<int, std::unique_ptr<Element>> _items;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree {

std::unique_ptr<Element> VirtualList::CreateElement() const {
    return std::make_unique<Internal::VirtualListElement>();
}

} // namespace ImFrame::Tree
