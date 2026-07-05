/**
 * @file     Combo.hpp
 * @brief    Drop-down combo box templated on item type
 *
 * `Combo<T>` builds display strings from `std::span<const T>` items and
 * delegates the ImGui popup rendering to a non-template internal helper so
 * that `<imgui.h>` is not pulled into this header.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
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

namespace ImFrame::Widgets {

/**
 * @class    Combo
 * @brief    Immediate-mode drop-down combo box for selecting from typed items
 *
 * @deprecated Use `ComboWidget<T>` instead (Phase 29). See `Docs/Migration_v1_to_v2.md`.
 *             Removed in Phase 30.
 *
 * Items are provided as a `std::span<const T>` for zero-copy access to any
 * contiguous container. An `ItemLabel` delegate converts each `T` to a display
 * string; if omitted the widget falls back to `std::format("{}", item)` for
 * formattable types.
 *
 * @tparam   T  Item type. Must be equality-comparable.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * static const std::string modes[] = {"Linear", "Nearest", "Cubic"};
 * std::string selectedMode = "Linear";
 * Widgets::Combo<std::string>("Filter", selectedMode, modes)
 *     .ItemLabel([](const std::string& s) { return s; })
 *     .Show();
 * @endcode
 */
template<typename T>
    requires std::equality_comparable<T>
class [[deprecated("Use ComboWidget<T> instead. See Docs/Migration_v1_to_v2.md.")]] Combo {
public:
    /**
     * @brief    Construct a combo box.
     * @param[in]   label     Display label.
     * @param[in,out]  selected  Currently selected item; updated when the user picks a new one.
     * @param[in]   items     View over the full item list. The span must remain valid until `Show()` returns.
     * @throws   Nothing.
     */
    explicit Combo(std::string_view label, T& selected, std::span<const T> items)
        : _label(label), _selected(selected), _items(items) {}

    /**
     * @brief    Provide a custom string renderer for each item.
     * @param[in]  fn  Delegate converting a `const T&` to a display `std::string`.
     * @return   `*this` for chaining.
     */
    Combo& ItemLabel(Utility::Delegate<std::string(const T&)> fn) { _itemLabel = std::move(fn); return *this; }
    Combo& Disabled(bool disabled = true)                         { _disabled = disabled;        return *this; }
    Combo& Tooltip(std::string_view tip)                          { _tooltip = tip;              return *this; }
    Combo& Width(float w)                                         { _width = w;                  return *this; }
    Combo& Id(std::string_view id)                                { _id = id;                    return *this; }

    /**
     * @brief    Render the combo popup and update `selected` if a new item is chosen.
     * @return   `true` if the selection changed this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show() {
        // Locate the current selection index.
        int selectedIdx = 0;
        for (std::size_t i = 0; i < _items.size(); ++i) {
            if (_items[i] == _selected) {
                selectedIdx = static_cast<int>(i);
                break;
            }
        }

        // Build display labels.
        std::vector<std::string> labels;
        labels.reserve(_items.size());
        for (const auto& item : _items) {
            if (_itemLabel) {
                labels.push_back(_itemLabel(item));
            } else if constexpr (std::formattable<T, char>) {
                labels.push_back(std::format("{}", item));
            } else {
                labels.push_back("[?]");
            }
        }

        bool changed = Internal::ShowComboImpl(_id, _label, selectedIdx, labels,
                                               _disabled, _tooltip, _width);
        if (changed) {
            _selected = _items[static_cast<std::size_t>(selectedIdx)];
        }
        return changed;
    }

private:
    std::string_view                            _label;
    T&                                          _selected;
    std::span<const T>                          _items;
    Utility::Delegate<std::string(const T&)>    _itemLabel;
    std::string_view                            _tooltip;
    std::string_view                            _id;
    float                                       _width    = 0.0f;
    bool                                        _disabled = false;
};

} // namespace ImFrame::Widgets

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

    void Paint(Widgets::Vec2 position) override {
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
