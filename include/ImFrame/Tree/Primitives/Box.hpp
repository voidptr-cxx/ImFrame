/**
 * @file     Box.hpp
 * @brief    Rectangular box primitive with sizing, padding, and decoration
 *
 * `Box` is the fundamental building block of the widget tree — every other
 * container `Component` is ultimately a `Box` composition.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <optional>

namespace ImFrame::Tree::Primitives {

/**
 * @class    Box
 * @brief    Sized, padded, optionally decorated container holding zero or one child
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * Box()
 *     .Padding(Widgets::EdgeInsets::All(8.0f))
 *     .Background({0.15f, 0.15f, 0.18f, 1.0f})
 *     .Radius(4.0f)
 *     .Child(Text("Hello"));
 * @endcode
 */
class Box {
public:
    Box() = default;

    Box& Width(float w) noexcept    { _width = w;     return *this; }
    Box& Height(float h) noexcept   { _height = h;    return *this; }
    Box& MinWidth(float w) noexcept  { _minWidth = w;  return *this; }
    Box& MinHeight(float h) noexcept { _minHeight = h; return *this; }
    Box& MaxWidth(float w) noexcept  { _maxWidth = w;  return *this; }
    Box& MaxHeight(float h) noexcept { _maxHeight = h; return *this; }

    /// Inner space reserved between this box's edges and its child.
    Box& Padding(Widgets::EdgeInsets insets) noexcept { _padding = insets; return *this; }

    /// Fill colour. Default `{0,0,0,0}` (transparent — no fill drawn).
    Box& Background(Widgets::Vec4 color) noexcept { _background = color; return *this; }

    /// Border colour. Default `{0,0,0,0}` (transparent — no border drawn).
    Box& BorderColor(Widgets::Vec4 color) noexcept { _borderColor = color; return *this; }

    /// Border stroke width in pixels. `0` (default) draws no border.
    Box& BorderWidth(float w) noexcept { _borderWidth = w; return *this; }

    /// Corner radius in pixels, applied to both fill and border.
    Box& Radius(float r) noexcept { _radius = r; return *this; }

    /// Optional single child widget.
    Box& Child(Widget child) { _child.emplace(std::move(child)); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    Box& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }

    [[nodiscard]] float GetWidth()  const noexcept { return _width; }
    [[nodiscard]] float GetHeight() const noexcept { return _height; }
    [[nodiscard]] float GetMinWidth()  const noexcept { return _minWidth; }
    [[nodiscard]] float GetMinHeight() const noexcept { return _minHeight; }
    [[nodiscard]] float GetMaxWidth()  const noexcept { return _maxWidth; }
    [[nodiscard]] float GetMaxHeight() const noexcept { return _maxHeight; }
    [[nodiscard]] Widgets::EdgeInsets GetPadding() const noexcept { return _padding; }
    [[nodiscard]] Widgets::Vec4 GetBackground() const noexcept { return _background; }
    [[nodiscard]] Widgets::Vec4 GetBorderColor() const noexcept { return _borderColor; }
    [[nodiscard]] float GetBorderWidth() const noexcept { return _borderWidth; }
    [[nodiscard]] float GetRadius() const noexcept { return _radius; }
    [[nodiscard]] const std::optional<Widget>& GetChild() const noexcept { return _child; }

    /// @internal Produces this box's concrete `Element`. Defined in `BoxRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    float _width  = -1.0f, _height  = -1.0f; ///< `< 0` = unconstrained on that axis.
    float _minWidth = 0.0f, _minHeight = 0.0f;
    float _maxWidth = std::numeric_limits<float>::max(), _maxHeight = std::numeric_limits<float>::max();
    Widgets::EdgeInsets _padding{};
    Widgets::Vec4        _background{0.0f, 0.0f, 0.0f, 0.0f};
    Widgets::Vec4        _borderColor{0.0f, 0.0f, 0.0f, 0.0f};
    float                _borderWidth = 0.0f;
    float                _radius      = 0.0f;
    std::optional<Widget> _child;
    Tree::Key             _key;
};

} // namespace ImFrame::Tree::Primitives
