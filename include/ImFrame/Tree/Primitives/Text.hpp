/**
 * @file     Text.hpp
 * @brief    Leaf text primitive
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

#include <string>
#include <string_view>

namespace ImFrame::Tree::Primitives {

/**
 * @class    Text
 * @brief    Renders a string with colour, size, wrap, and alignment options
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * Text("Hello, ImFrame!").Color({1, 1, 1, 1}).Align(Widgets::TextAlign::Center);
 * @endcode
 */
class Text {
public:
    explicit Text(std::string_view content) : _content(content) {}

    Text& Content(std::string_view content) { _content = content; return *this; }
    Text& Color(Widgets::Vec4 color) noexcept { _color = color; return *this; }

    /// Pixel font size. `0` (default) uses the current ImGui font's native size.
    Text& FontSize(float size) noexcept { _fontSize = size; return *this; }

    /// Wrap at the available width supplied by the parent's layout constraints.
    Text& Wrap(bool wrap = true) noexcept { _wrap = wrap; return *this; }

    Text& Align(Widgets::TextAlign align) noexcept { _align = align; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    Text& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }

    [[nodiscard]] const std::string&  GetContent() const noexcept { return _content; }
    [[nodiscard]] Widgets::Vec4       GetColor() const noexcept { return _color; }
    [[nodiscard]] float               GetFontSize() const noexcept { return _fontSize; }
    [[nodiscard]] bool                GetWrap() const noexcept { return _wrap; }
    [[nodiscard]] Widgets::TextAlign  GetAlign() const noexcept { return _align; }

    /// @internal Produces this text's concrete `Element`. Defined in `TextRO.cpp`.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    std::string         _content;
    Widgets::Vec4        _color{1.0f, 1.0f, 1.0f, 1.0f};
    float                _fontSize = 0.0f;
    bool                 _wrap     = false;
    Widgets::TextAlign  _align    = Widgets::TextAlign::Start;
    Tree::Key            _key;
};

} // namespace ImFrame::Tree::Primitives
