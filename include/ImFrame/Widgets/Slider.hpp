/**
 * @file     Slider.hpp
 * @brief    Numeric range slider templated on arithmetic types
 *
 * `Slider<T>` dispatches to `ImGui::SliderInt` for `int`, `ImGui::SliderFloat`
 * for `float`, and a double-precision scalar path for all other arithmetic types.
 * The ImGui calls are delegated to non-template internal helpers so that
 * `<imgui.h>` is not pulled into this header.
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

bool ShowSliderInt(std::string_view id, std::string_view label,
                   int& value, int min, int max,
                   std::string_view format, bool disabled,
                   std::string_view tooltip, float width);

bool ShowSliderFloat(std::string_view id, std::string_view label,
                     float& value, float min, float max,
                     std::string_view format, bool disabled,
                     std::string_view tooltip, float width);

bool ShowSliderDouble(std::string_view id, std::string_view label,
                      double& value, double min, double max,
                      std::string_view format, bool disabled,
                      std::string_view tooltip, float width);

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

/**
 * @class    Slider
 * @brief    Immediate-mode slider bound to an arithmetic value with min/max range
 *
 * @deprecated Use `SliderWidget<T>` instead (Phase 29). See `Docs/Migration_v1_to_v2.md`.
 *             Removed in Phase 30.
 *
 * @tparam   T  Any arithmetic type. `int` and `float` map directly to ImGui sliders;
 *              integral types map to `int`; floating-point types map to `double`.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * float volume = 0.8f;
 * Widgets::Slider<float>("Volume", volume, 0.0f, 1.0f)
 *     .Format("%.2f")
 *     .Width(200.0f)
 *     .Show();
 * @endcode
 */
template<typename T>
    requires std::is_arithmetic_v<T>
class [[deprecated("Use SliderWidget<T> instead. See Docs/Migration_v1_to_v2.md.")]] Slider {
public:
    /**
     * @brief    Construct a slider.
     * @param[in]   label  Display label.
     * @param[in,out]  value  Bound value; clamped to `[min, max]` on user interaction.
     * @param[in]   min    Lower bound (inclusive).
     * @param[in]   max    Upper bound (inclusive).
     * @throws   Nothing.
     */
    explicit Slider(std::string_view label, T& value, T min, T max)
        : _label(label), _value(value), _min(min), _max(max) {}

    /**
     * @brief    Override the printf-style display format for the current value.
     * @param[in]  format  Format string (e.g. `"%.2f"`, `"%d%%"`). Empty = ImGui default.
     * @return   `*this` for chaining.
     */
    Slider& Format(std::string_view format) { _format = format;   return *this; }
    Slider& Disabled(bool disabled = true)  { _disabled = disabled; return *this; }
    Slider& Tooltip(std::string_view tip)   { _tooltip = tip;      return *this; }
    Slider& Width(float w)                  { _width = w;          return *this; }
    Slider& Id(std::string_view id)         { _id = id;            return *this; }

    /**
     * @brief    Render the slider and update `value` if dragged.
     * @return   `true` if the value changed this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show() {
        if constexpr (std::is_same_v<T, int>) {
            return Internal::ShowSliderInt(_id, _label, _value, _min, _max,
                                           _format, _disabled, _tooltip, _width);
        } else if constexpr (std::is_same_v<T, float>) {
            return Internal::ShowSliderFloat(_id, _label, _value, _min, _max,
                                             _format, _disabled, _tooltip, _width);
        } else if constexpr (std::is_integral_v<T>) {
            // Promote narrow/wide ints to int for the slider call.
            int v  = static_cast<int>(_value);
            int mn = static_cast<int>(_min);
            int mx = static_cast<int>(_max);
            bool changed = Internal::ShowSliderInt(_id, _label, v, mn, mx,
                                                    _format, _disabled, _tooltip, _width);
            if (changed) { _value = static_cast<T>(v); }
            return changed;
        } else {
            // All remaining floating-point types (double, long double) → double.
            double v  = static_cast<double>(_value);
            double mn = static_cast<double>(_min);
            double mx = static_cast<double>(_max);
            bool changed = Internal::ShowSliderDouble(_id, _label, v, mn, mx,
                                                       _format, _disabled, _tooltip, _width);
            if (changed) { _value = static_cast<T>(v); }
            return changed;
        }
    }

private:
    std::string_view    _label;
    T&                  _value;
    T                   _min;
    T                   _max;
    std::string_view    _format;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

} // namespace ImFrame::Widgets

namespace ImFrame::Internal {

/// @internal Concrete `Element` backing `Widgets::SliderWidget<T>`. Must live in a public header — see file comment.
template<typename T>
class SliderWidgetElement;

} // namespace ImFrame::Internal

namespace ImFrame::Widgets {

// ─── SliderWidget<T> (Phase 29) ─────────────────────────────────────────────────

/**
 * @class    SliderWidget
 * @brief    Declarative slider — `Tree::PrimitiveWidget` replacement for `Slider<T>`
 *
 * Binds to caller-owned storage via a raw pointer, mirroring `CheckboxWidget`.
 * Adds `OnChange` (absent from the old imperative `Slider<T>`, which only
 * returned a `bool` from `Show()`) since a declarative API has no natural place
 * for callers to poll a per-frame return value.
 *
 * @tparam   T  Any arithmetic type — same dispatch rules as `Slider<T>`.
 * @since    2.4.0
 */
template<typename T>
    requires std::is_arithmetic_v<T>
class SliderWidget {
public:
    /// `value` must outlive this widget and every `Element` mounted from it.
    SliderWidget(std::string label, T* value, T min, T max)
        : _label(std::move(label)), _value(value), _min(min), _max(max) {}

    SliderWidget& Format(std::string format) { _format = std::move(format); return *this; }
    SliderWidget& OnChange(Utility::Delegate<void(T)> cb) { _onChange = std::move(cb); return *this; }
    SliderWidget& Disabled(bool disabled = true) noexcept { _disabled = disabled; return *this; }
    SliderWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }
    SliderWidget& Width(float w) noexcept { _width = w; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    SliderWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetLabel() const noexcept { return _label; }
    [[nodiscard]] T* GetValue() const noexcept { return _value; }
    [[nodiscard]] T GetMin() const noexcept { return _min; }
    [[nodiscard]] T GetMax() const noexcept { return _max; }
    [[nodiscard]] const std::string& GetFormat() const noexcept { return _format; }
    [[nodiscard]] const Utility::Delegate<void(T)>& GetOnChange() const noexcept { return _onChange; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }
    [[nodiscard]] float GetWidth() const noexcept { return _width; }

    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const {
        return std::make_unique<Internal::SliderWidgetElement<T>>();
    }

private:
    std::string               _label;
    T*                        _value = nullptr;
    T                         _min;
    T                         _max;
    std::string               _format;
    Utility::Delegate<void(T)> _onChange;
    bool                      _disabled = false;
    std::string               _tooltip;
    float                     _width = 0.0f;
    Tree::Key                 _key;
};

} // namespace ImFrame::Widgets

namespace ImFrame::Internal {

template<typename T>
class SliderWidgetElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<Widgets::SliderWidget<T>>();
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<Widgets::SliderWidget<T>>();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        _size = constraints.Constrain(MeasureControlSize(_config.GetWidth()));
        return _size;
    }

    void Paint(Widgets::Vec2 position) override {
        BeginControlPaint(this, position);

        T* value = _config.GetValue();
        if (value) {
            bool changed = false;
            if constexpr (std::is_same_v<T, int>) {
                changed = ShowSliderInt("", _config.GetLabel(), *value, _config.GetMin(), _config.GetMax(),
                                        _config.GetFormat(), _config.GetDisabled(), _config.GetTooltip(),
                                        _config.GetWidth());
            } else if constexpr (std::is_same_v<T, float>) {
                changed = ShowSliderFloat("", _config.GetLabel(), *value, _config.GetMin(), _config.GetMax(),
                                          _config.GetFormat(), _config.GetDisabled(), _config.GetTooltip(),
                                          _config.GetWidth());
            } else if constexpr (std::is_integral_v<T>) {
                int v  = static_cast<int>(*value);
                int mn = static_cast<int>(_config.GetMin());
                int mx = static_cast<int>(_config.GetMax());
                changed = ShowSliderInt("", _config.GetLabel(), v, mn, mx, _config.GetFormat(),
                                        _config.GetDisabled(), _config.GetTooltip(), _config.GetWidth());
                if (changed) { *value = static_cast<T>(v); }
            } else {
                double v  = static_cast<double>(*value);
                double mn = static_cast<double>(_config.GetMin());
                double mx = static_cast<double>(_config.GetMax());
                changed = ShowSliderDouble("", _config.GetLabel(), v, mn, mx, _config.GetFormat(),
                                           _config.GetDisabled(), _config.GetTooltip(), _config.GetWidth());
                if (changed) { *value = static_cast<T>(v); }
            }
            if (changed && _config.GetOnChange()) { _config.GetOnChange()(*value); }
        }

        EndControlPaint();
    }

private:
    Widgets::SliderWidget<T> _config{"", nullptr, T{}, T{}};
};

} // namespace ImFrame::Internal
