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

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Utility/Delegate.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <memory>
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
 * @deprecated Use `TextInputWidget<T>` instead (Phase 29). See `Docs/Migration_v1_to_v2.md`.
 *             Removed in Phase 30.
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
class [[deprecated("Use TextInputWidget<T> instead. See Docs/Migration_v1_to_v2.md.")]] TextInput {
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

namespace ImFrame::Internal {

/// @internal Concrete `Element` backing `Widgets::TextInputWidget<T>`. Must live in a public header — see file comment.
template<typename T>
class TextInputWidgetElement;

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

// ─── TextInputWidget<T> (Phase 29) ──────────────────────────────────────────────

/**
 * @class    TextInputWidget
 * @brief    Declarative text input — `Tree::PrimitiveWidget` replacement for `TextInput<T>`
 *
 * Binds to caller-owned storage via a raw pointer, mirroring `CheckboxWidget`/`SliderWidget`.
 *
 * @tparam   T  Either `std::string` or `std::u8string`.
 * @since    2.4.0
 */
template<typename T>
    requires (std::is_same_v<T, std::string> || std::is_same_v<T, std::u8string>)
class TextInputWidget {
public:
    /// `value` must outlive this widget and every `Element` mounted from it.
    TextInputWidget(std::string label, T* value) : _label(std::move(label)), _value(value) {}

    TextInputWidget& Hint(std::string hint) { _hint = std::move(hint); return *this; }
    TextInputWidget& Multiline(bool multiline = true) noexcept { _multiline = multiline; return *this; }
    TextInputWidget& Password(bool password = true) noexcept { _password = password; return *this; }
    TextInputWidget& OnChange(Utility::Delegate<void(const T&)> cb) { _onChange = std::move(cb); return *this; }
    TextInputWidget& Disabled(bool disabled = true) noexcept { _disabled = disabled; return *this; }
    TextInputWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }
    TextInputWidget& Width(float w) noexcept { _width = w; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    TextInputWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetLabel() const noexcept { return _label; }
    [[nodiscard]] T* GetValue() const noexcept { return _value; }
    [[nodiscard]] const std::string& GetHint() const noexcept { return _hint; }
    [[nodiscard]] bool GetMultiline() const noexcept { return _multiline; }
    [[nodiscard]] bool GetPassword() const noexcept { return _password; }
    [[nodiscard]] const Utility::Delegate<void(const T&)>& GetOnChange() const noexcept { return _onChange; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }
    [[nodiscard]] float GetWidth() const noexcept { return _width; }

    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const {
        return std::make_unique<Internal::TextInputWidgetElement<T>>();
    }

private:
    std::string                        _label;
    T*                                 _value = nullptr;
    std::string                        _hint;
    bool                               _multiline = false;
    bool                               _password  = false;
    Utility::Delegate<void(const T&)>  _onChange;
    bool                               _disabled = false;
    std::string                        _tooltip;
    float                              _width = 0.0f;
    Tree::Key                          _key;
};

} // namespace ImFrame::Widgets

namespace ImFrame::Internal {

template<typename T>
class TextInputWidgetElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::TextInputWidget<T>>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::TextInputWidget<T>>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        Widgets::Vec2 size = MeasureControlSize(_config.GetWidth());
        if (_config.GetMultiline()) { size.y = 100.0f; } // matches ShowTextInputStr's fixed multiline height
        _size = constraints.Constrain(size);
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        BeginControlPaint(this, position);

        T* value = _config.GetValue();
        if (value) {
            bool changed;
            if constexpr (std::is_same_v<T, std::string>) {
                changed = ShowTextInputStr("", _config.GetLabel(), *value, _config.GetHint(),
                                           _config.GetMultiline(), _config.GetPassword(),
                                           _config.GetDisabled(), _config.GetTooltip(), _config.GetWidth());
            } else {
                changed = ShowTextInputU8("", _config.GetLabel(), *value, _config.GetHint(),
                                          _config.GetMultiline(), _config.GetPassword(),
                                          _config.GetDisabled(), _config.GetTooltip(), _config.GetWidth());
            }
            if (changed && _config.GetOnChange()) { _config.GetOnChange()(*value); }
        }

        EndControlPaint();
    }

private:
    Widgets::TextInputWidget<T> _config{"", nullptr};
};

} // namespace ImFrame::Internal
