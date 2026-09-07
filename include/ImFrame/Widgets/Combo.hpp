/**
 * @file     Combo.hpp
 * @brief    Drop-down combo box templated on item type
 *
 * `ComboWidget<T>` builds display strings from `std::span<const T>` items and
 * delegates the ImGui popup rendering to a non-template internal helper so
 * that `<imgui.h>` is not pulled into this header.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// ─── Internal helper (declared here for template Show() — not part of public API) ─
namespace ImFrame::Internal {

bool ShowComboImpl(std::string_view id, std::string_view label,
                   int& selectedIdx,
                   std::span<const std::string> labels,
                   bool disabled, std::string_view tooltip, float width);

} // namespace ImFrame::Internal

namespace ImFrame::Internal {

/// @internal Concrete `Element` backing `Widgets::ComboWidget<T>`. Must live in a public header — see file comment.
template<typename T>
class ComboWidgetElement;

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

// ─── ComboWidget<T> (Phase 29) ──────────────────────────────────────────────────

/**
 * @class    ComboWidget
 * @brief    Declarative combo box — `Tree::PrimitiveWidget` replacement for `Combo<T>`
 *
 * ImGui's native combo popup (`BeginCombo`/`Selectable`/`EndCombo`) already
 * renders on its own overlay draw list, independent of the parent window's
 * clip rect — unlike `Button`/`Checkbox`/`Slider`/`TextInput`, a combo box does
 * not need `Portal` to escape clipping. Binds to caller-owned storage via a
 * raw pointer, mirroring `CheckboxWidget`.
 *
 * @tparam   T  Item type. Must be equality-comparable.
 * @since    2.4.0
 */
template<typename T>
    requires std::equality_comparable<T>
class ComboWidget {
public:
    /// `selected` must outlive this widget and every `Element` mounted from it. `items` must remain valid the same way.
    ComboWidget(std::string label, T* selected, std::span<const T> items)
        : _label(std::move(label)), _selected(selected), _items(items) {}

    ComboWidget& ItemLabel(Utility::Delegate<std::string(const T&)> fn) { _itemLabel = std::move(fn); return *this; }
    ComboWidget& OnChange(Utility::Delegate<void(const T&)> cb) { _onChange = std::move(cb); return *this; }
    ComboWidget& Disabled(bool disabled = true) noexcept { _disabled = disabled; return *this; }
    ComboWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }
    ComboWidget& Width(float w) noexcept { _width = w; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    ComboWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetLabel() const noexcept { return _label; }
    [[nodiscard]] T* GetSelected() const noexcept { return _selected; }
    [[nodiscard]] std::span<const T> GetItems() const noexcept { return _items; }
    [[nodiscard]] const Utility::Delegate<std::string(const T&)>& GetItemLabel() const noexcept { return _itemLabel; }
    [[nodiscard]] const Utility::Delegate<void(const T&)>& GetOnChange() const noexcept { return _onChange; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }
    [[nodiscard]] float GetWidth() const noexcept { return _width; }

    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const {
        return std::make_unique<Internal::ComboWidgetElement<T>>();
    }

private:
    std::string                                _label;
    T*                                          _selected = nullptr;
    std::span<const T>                          _items;
    Utility::Delegate<std::string(const T&)>    _itemLabel;
    Utility::Delegate<void(const T&)>           _onChange;
    bool                                        _disabled = false;
    std::string                                 _tooltip;
    float                                        _width = 0.0f;
    Tree::Key                                    _key;
};

} // namespace ImFrame::Widgets

namespace ImFrame::Internal {

template<typename T>
class ComboWidgetElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::ComboWidget<T>>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::ComboWidget<T>>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        _size = constraints.Constrain(MeasureControlSize(_config.GetWidth()));
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 position) override {
        BeginControlPaint(this, position);

        T* selected = _config.GetSelected();
        if (selected) {
            const std::span<const T> items = _config.GetItems();

            int selectedIdx = 0;
            for (std::size_t i = 0; i < items.size(); ++i) {
                if (items[i] == *selected) {
                    selectedIdx = static_cast<int>(i);
                    break;
                }
            }

            std::vector<std::string> labels;
            labels.reserve(items.size());
            for (const auto& item : items) {
                if (_config.GetItemLabel()) {
                    labels.push_back(_config.GetItemLabel()(item));
                } else if constexpr (std::formattable<T, char>) {
                    labels.push_back(std::format("{}", item));
                } else {
                    labels.push_back("[?]");
                }
            }

            const bool changed = ShowComboImpl("", _config.GetLabel(), selectedIdx, labels,
                                               _config.GetDisabled(), _config.GetTooltip(), _config.GetWidth());
            if (changed) {
                *selected = items[static_cast<std::size_t>(selectedIdx)];
                if (_config.GetOnChange()) { _config.GetOnChange()(*selected); }
            }
        }

        EndControlPaint();
    }

private:
    Widgets::ComboWidget<T> _config{"", nullptr, std::span<const T>{}};
};

} // namespace ImFrame::Internal
