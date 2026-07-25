/**
 * @file     TextInput.hpp
 * @brief    Text input widget templated on `std::string` and `std::u8string`
 *
 * `TextInputWidget<T>` is constrained to `std::string` and `std::u8string`. The ImGui
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

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 position) override {
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
