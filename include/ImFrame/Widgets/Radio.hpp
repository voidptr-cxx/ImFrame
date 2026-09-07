/**
 * @file     Radio.hpp
 * @brief    Radio button widget for mutually exclusive option selection
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
#include "ImFrame/Widgets/Types.hpp"

#include <string>

namespace ImFrame::Widgets {

// ─── RadioWidget (Phase 30) ─────────────────────────────────────────────────────

/**
 * @class    RadioWidget
 * @brief    Declarative radio button — `Tree::PrimitiveWidget` replacement for `Radio`
 *
 * Binds to caller-owned storage via a raw pointer (not a reference — mirrors
 * `CheckboxWidget`'s `bool*` reasoning). Multiple `RadioWidget`s bound to the
 * same `int*` form a group. Produces the exact same
 * `ImGui::RadioButton(label, int*, int)` call as `Radio::Show()`.
 *
 * @since    2.5.0
 *
 * @example
 * @code
 * int mode = 0;
 * RadioWidget("Linear",  &mode, 0);
 * RadioWidget("Nearest", &mode, 1);
 * @endcode
 */
class RadioWidget {
public:
    /// `value` must outlive this widget and every `Element` mounted from it.
    RadioWidget(std::string label, int* value, int option)
        : _label(std::move(label)), _value(value), _option(option) {}

    RadioWidget& Disabled(bool disabled = true) noexcept { _disabled = disabled; return *this; }
    RadioWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }
    RadioWidget& Width(float w) noexcept { _width = w; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    RadioWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetLabel() const noexcept { return _label; }
    [[nodiscard]] int* GetValue() const noexcept { return _value; }
    [[nodiscard]] int GetOption() const noexcept { return _option; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }
    [[nodiscard]] float GetWidth() const noexcept { return _width; }

    /// @internal Produces this radio button's concrete `Element`. Defined in `Radio.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    std::string _label;
    int*        _value = nullptr;
    int         _option;
    bool        _disabled = false;
    std::string _tooltip;
    float       _width    = 0.0f;
    Tree::Key   _key;
};

} // namespace ImFrame::Widgets
