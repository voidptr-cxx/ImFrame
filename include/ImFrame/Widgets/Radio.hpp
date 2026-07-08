/**
 * @file     Radio.hpp
 * @brief    Radio button widget for mutually exclusive option selection
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
#include "ImFrame/Widgets/Types.hpp"

#include <string>
#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    Radio
 * @brief    Single radio button in a group sharing one `int&` binding
 *
 * Multiple `Radio` instances bound to the same `int&` form a radio group.
 * Each instance represents one option identified by an integer value.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * int mode = 0;
 * Widgets::Radio("Linear",  mode, 0).Show();
 * Widgets::Radio("Nearest", mode, 1).Show();
 * Widgets::Radio("Cubic",   mode, 2).Show();
 * @endcode
 */
/// @deprecated Use `RadioWidget` instead (Phase 30). See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.2.
class [[deprecated("Use RadioWidget instead. See Docs/Migration_v1_to_v2.md.")]] Radio {
public:
    /**
     * @brief    Construct a radio button.
     * @param[in]   label   Text label displayed next to the radio button.
     * @param[in,out]  value   Shared selection state; set to `option` when this button is selected.
     * @param[in]   option  The integer value this radio button represents.
     * @throws   Nothing.
     */
    explicit Radio(std::string_view label, int& value, int option)
        : _label(label), _value(value), _option(option) {}

    Radio& Disabled(bool disabled = true)  { _disabled = disabled;  return *this; }
    Radio& Tooltip(std::string_view tip)   { _tooltip = tip;         return *this; }
    Radio& Width(float w)                  { _width = w;             return *this; }
    Radio& Id(std::string_view id)         { _id = id;               return *this; }

    /**
     * @brief    Render the radio button and update `value` if selected.
     * @return   `true` if this option was selected this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view    _label;
    int&                _value;
    int                 _option;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

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
