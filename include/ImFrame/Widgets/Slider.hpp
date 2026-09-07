/**
 * @file     Slider.hpp
 * @brief    Numeric range slider templated on arithmetic types
 *
 * `SliderWidget<T>` dispatches to `ImGui::SliderInt` for `int`, `ImGui::SliderFloat`
 * for `float`, and a double-precision scalar path for all other arithmetic types.
 * The ImGui calls are delegated to non-template internal helpers so that
 * `<imgui.h>` is not pulled into this header.
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

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 position) override {
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
