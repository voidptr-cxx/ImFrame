/**
 * @file     Panel.hpp
 * @brief    Bordered child-window container built on `ImGui::BeginChild`
 *
 * `Panel` wraps `ImGui::BeginChild` / `ImGui::EndChild` with a fluent builder
 * API. Optional padding and background colour are applied via ImGui style
 * variables pushed around the `BeginChild` call; no Phase-2 scope wrappers are
 * required.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Layout/ChildScope.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <string_view>

namespace ImFrame::Layout {

/**
 * @class    Panel
 * @brief    Fluent-builder panel wrapping `ImGui::BeginChild` / `ImGui::EndChild`
 *
 * @since    1.1.0
 *
 * @example
 * @code
 * if (auto scope = Panel("sidebar")
 *         .Size({220.0f, 0.0f})
 *         .Border(true)
 *         .Begin()) {
 *     Widgets::Text("Hello").Show();
 * }
 * @endcode
 */
class Panel {
public:
    /**
     * @brief    Construct a panel with the given ImGui child-window ID.
     * @param[in]  id  Unique string ID passed to `ImGui::BeginChild()`.
     * @throws   Nothing.
     */
    explicit Panel(std::string_view id) noexcept : _id(id) {}

    Panel& Size(Widgets::Vec2 size) noexcept        { _size = size;        return *this; }
    Panel& Border(bool border = true) noexcept      { _border = border;    return *this; }

    /**
     * @brief    Override the child window's inner padding.
     * @param[in]  padding  Padding in pixels pushed via `ImGuiStyleVar_WindowPadding`.
     * @return   `*this` for chaining.
     */
    Panel& Padding(Widgets::Vec2 padding) noexcept  { _padding = padding;  _hasPadding = true; return *this; }

    /**
     * @brief    Set the child window's background colour.
     * @param[in]  color  RGBA colour pushed via `ImGuiCol_ChildBg`.
     * @return   `*this` for chaining.
     */
    Panel& Background(Widgets::Vec4 color) noexcept { _bg = color;         _hasBg = true;      return *this; }

    /**
     * @brief    Open the child window and return an RAII scope.
     * @return   `ChildScope` that evaluates as `true` when the panel is visible.
     *           The scope's destructor always calls `ImGui::EndChild()`.
     * @throws   Nothing.
     */
    [[nodiscard]] ChildScope Begin();

private:
    std::string_view _id;
    Widgets::Vec2    _size       {};
    Widgets::Vec2    _padding    {};
    Widgets::Vec4    _bg         {};
    bool             _border     = true;
    bool             _hasPadding = false;
    bool             _hasBg      = false;
};

} // namespace ImFrame::Layout
