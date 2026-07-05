/**
 * @file     Button.hpp
 * @brief    Push-button widget with optional icon and explicit size
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

#include <string>
#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    Button
 * @brief    Immediate-mode push button with fluent builder API
 *
 * @deprecated Use `ButtonWidget` instead (Phase 29). See `Docs/Migration_v1_to_v2.md`.
 *             Removed in Phase 30.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Button("Save")
 *     .Icon(ImFrame::Icons::Fa::FloppyDisk)
 *     .Size({120.0f, 0.0f})
 *     .OnClick([&] { Save(); })
 *     .Show();
 * @endcode
 */
class [[deprecated("Use ButtonWidget instead. See Docs/Migration_v1_to_v2.md.")]] Button {
public:
    /**
     * @brief    Construct a button with the given label.
     * @param[in]  label  Text displayed on the button face.
     * @throws   Nothing.
     */
    explicit Button(std::string_view label) : _label(label) {}

    Button& Icon(const char* glyph)                        { _icon = glyph;               return *this; }
    Button& Size(Vec2 size)                                { _size = size;                 return *this; }

    /**
     * @brief    Register a click callback fired when the button is clicked.
     * @param[in]  cb  Zero-argument delegate. Must fit in the 16-byte Delegate buffer.
     * @return   `*this` for chaining.
     */
    Button& OnClick(Utility::Delegate<void()> cb)          { _onClick = std::move(cb);    return *this; }

    Button& Disabled(bool disabled = true)                 { _disabled = disabled;         return *this; }
    Button& Tooltip(std::string_view tip)                  { _tooltip = tip;               return *this; }
    Button& Width(float w)                                 { _width = w;                   return *this; }
    Button& Id(std::string_view id)                        { _id = id;                     return *this; }

    /**
     * @brief    Render the button and fire `OnClick` if clicked.
     * @return   `true` if the button was clicked this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view            _label;
    const char*                 _icon     = nullptr;
    Vec2                        _size     {};
    Utility::Delegate<void()>   _onClick;
    std::string_view            _tooltip;
    std::string_view            _id;
    float                       _width    = 0.0f;
    bool                        _disabled = false;
};

// ─── ButtonWidget (Phase 29) ────────────────────────────────────────────────────

/**
 * @class    ButtonWidget
 * @brief    Declarative push button — `Tree::PrimitiveWidget` replacement for `Button`
 *
 * Produces the exact same `ImGui::Button()` call as `Button::Show()` (same visual
 * output), reached through `Internal::ButtonElement` instead of an imperative
 * `Show()` call. Has no internal reactive state — `OnClick` fires a `Delegate`
 * exactly like the old API; there is no hover-state persistence concern because
 * ImGui's own style system (`ImGuiCol_Button`/`ButtonHovered`/`ButtonActive`)
 * handles the hover/press visual entirely inside the single `ImGui::Button()` call.
 *
 * @since    2.4.0
 *
 * @example
 * @code
 * ButtonWidget("Save").OnClick([&] { Save(); });
 * @endcode
 */
class ButtonWidget {
public:
    explicit ButtonWidget(std::string label) : _label(std::move(label)) {}

    ButtonWidget& Icon(std::string glyph) { _icon = std::move(glyph); return *this; }
    ButtonWidget& Size(Vec2 size) noexcept { _size = size; return *this; }
    ButtonWidget& OnClick(Utility::Delegate<void()> cb) { _onClick = std::move(cb); return *this; }
    ButtonWidget& Disabled(bool disabled = true) noexcept { _disabled = disabled; return *this; }
    ButtonWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }
    ButtonWidget& Width(float w) noexcept { _width = w; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    ButtonWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetLabel() const noexcept { return _label; }
    [[nodiscard]] const std::string& GetIcon() const noexcept { return _icon; }
    [[nodiscard]] Vec2 GetSize() const noexcept { return _size; }
    [[nodiscard]] const Utility::Delegate<void()>& GetOnClick() const noexcept { return _onClick; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }
    [[nodiscard]] float GetWidth() const noexcept { return _width; }

    /// @internal Produces this button's concrete `Element`. Defined in `Button.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    std::string                _label;
    std::string                _icon;
    Vec2                       _size{};
    Utility::Delegate<void()>  _onClick;
    bool                       _disabled = false;
    std::string                _tooltip;
    float                      _width = 0.0f;
    Tree::Key                  _key;
};

} // namespace ImFrame::Widgets
