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

#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <format>
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
class Combo {
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
