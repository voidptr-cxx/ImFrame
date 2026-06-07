/**
 * @file     TextInput.hpp
 * @brief    Text input widget templated on `std::string` and `std::u8string`
 *
 * `TextInput<T>` is constrained to `std::string` and `std::u8string`. The ImGui
 * calls are delegated to non-template internal helpers defined in
 * `src/Widgets/WidgetImpl.cpp` so that `<imgui.h>` is not pulled into this header.
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

#include <string>
#include <string_view>
#include <type_traits>

// ─── Internal helpers (declared here for template Show() — not part of public API) ─
namespace ImFrame::Internal {

bool ShowTextInputStr(std::string_view id, std::string_view label, std::string& value,
                      std::string_view hint, bool multiline, bool password,
                      bool disabled, std::string_view tooltip, float width);

bool ShowTextInputU8(std::string_view id, std::string_view label, std::u8string& value,
                     std::string_view hint, bool multiline, bool password,
                     bool disabled, std::string_view tooltip, float width);

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

/**
 * @class    TextInput
 * @brief    Single-line or multi-line text input bound to a `std::string` or `std::u8string`
 *
 * @tparam   T  Either `std::string` or `std::u8string`.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * std::string name;
 * Widgets::TextInput<std::string>("Name", name)
 *     .Hint("Enter your name")
 *     .OnChange([&](const std::string& s) { model.name = s; })
 *     .Show();
 * @endcode
 */
template<typename T>
    requires (std::is_same_v<T, std::string> || std::is_same_v<T, std::u8string>)
class TextInput {
public:
    /**
     * @brief    Construct a text input bound to the given string.
     * @param[in]   label  Display label shown to the left of the field.
     * @param[in,out]  value  String modified in-place when the user edits.
     * @throws   Nothing.
     */
    explicit TextInput(std::string_view label, T& value) : _label(label), _value(value) {}

    /**
     * @brief    Show placeholder text when the field is empty.
     * @param[in]  hint  Hint text (grayed-out when field is empty).
     * @return   `*this` for chaining.
     */
    TextInput& Hint(std::string_view hint)             { _hint = hint;                  return *this; }
    TextInput& Multiline(bool multiline = true)        { _multiline = multiline;         return *this; }
    TextInput& Password(bool password = true)          { _password = password;           return *this; }

    /**
     * @brief    Register a change callback fired when the text changes.
     * @param[in]  cb  Delegate receiving a const reference to the new value.
     * @return   `*this` for chaining.
     */
    TextInput& OnChange(Utility::Delegate<void(const T&)> cb) { _onChange = std::move(cb); return *this; }

    TextInput& Disabled(bool disabled = true)          { _disabled = disabled;           return *this; }
    TextInput& Tooltip(std::string_view tip)           { _tooltip = tip;                 return *this; }
    TextInput& Width(float w)                          { _width = w;                     return *this; }
    TextInput& Id(std::string_view id)                 { _id = id;                       return *this; }

    /**
     * @brief    Render the text input and update `value` if edited.
     * @return   `true` if the value changed this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show() {
        bool changed;
        if constexpr (std::is_same_v<T, std::string>) {
            changed = Internal::ShowTextInputStr(_id, _label, _value, _hint,
                                                 _multiline, _password,
                                                 _disabled, _tooltip, _width);
        } else {
            changed = Internal::ShowTextInputU8(_id, _label, _value, _hint,
                                                _multiline, _password,
                                                _disabled, _tooltip, _width);
        }
        if (changed && _onChange) { _onChange(_value); }
        return changed;
    }

private:
    std::string_view                    _label;
    T&                                  _value;
    std::string_view                    _hint;
    Utility::Delegate<void(const T&)>   _onChange;
    std::string_view                    _tooltip;
    std::string_view                    _id;
    float                               _width    = 0.0f;
    bool                                _multiline = false;
    bool                                _password  = false;
    bool                                _disabled  = false;
};

} // namespace ImFrame::Widgets
