/**
 * @file     ColorEdit.hpp
 * @brief    RGBA colour picker widget bound to a Vec4 reference
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
 * @class    ColorEdit
 * @brief    Immediate-mode RGBA colour editor bound to a `Vec4&` reference
 *
 * `Vec4` fields map to `{r, g, w, b, a}` in normalised `[0, 1]` range.
 * The alpha channel is hidden by default; call `Alpha(true)` to expose it.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Vec4 tint{1.0f, 0.5f, 0.0f, 1.0f};
 * Widgets::ColorEdit("Tint", tint)
 *     .Alpha(true)
 *     .OnChange([&](Vec4 c) { material.SetTint(c); })
 *     .Show();
 * @endcode
 */
/// @deprecated Use `ColorEditWidget` instead (Phase 30). See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.2.
class [[deprecated("Use ColorEditWidget instead. See Docs/Migration_v1_to_v2.md.")]] ColorEdit {
public:
    /**
     * @brief    Construct a colour editor bound to a Vec4.
     * @param[in]   label  Display label.
     * @param[in,out]  value  RGBA colour in `[0, 1]` normalised range. Modified in-place.
     * @throws   Nothing.
     */
    explicit ColorEdit(std::string_view label, Vec4& value) : _label(label), _value(value) {}

    /**
     * @brief    Show or hide the alpha channel slider.
     * @param[in]  alpha  `true` = show alpha (default: false).
     * @return   `*this` for chaining.
     */
    ColorEdit& Alpha(bool alpha = true)                        { _alpha = alpha;            return *this; }

    /**
     * @brief    Register a change callback fired when the colour changes.
     * @param[in]  cb  Delegate receiving a copy of the new colour value.
     * @return   `*this` for chaining.
     */
    ColorEdit& OnChange(Utility::Delegate<void(Vec4)> cb)      { _onChange = std::move(cb); return *this; }

    ColorEdit& Disabled(bool disabled = true)                  { _disabled = disabled;      return *this; }
    ColorEdit& Tooltip(std::string_view tip)                   { _tooltip = tip;            return *this; }
    ColorEdit& Width(float w)                                  { _width = w;                return *this; }
    ColorEdit& Id(std::string_view id)                         { _id = id;                  return *this; }

    /**
     * @brief    Render the colour editor and update `value` if changed.
     * @return   `true` if the colour changed this frame; `false` otherwise.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view                _label;
    Vec4&                           _value;
    Utility::Delegate<void(Vec4)>   _onChange;
    std::string_view                _tooltip;
    std::string_view                _id;
    float                           _width    = 0.0f;
    bool                            _alpha    = false;
    bool                            _disabled = false;
};

// ─── ColorEditWidget (Phase 30) ─────────────────────────────────────────────────

/**
 * @class    ColorEditWidget
 * @brief    Declarative RGBA colour editor — `Tree::PrimitiveWidget` replacement for `ColorEdit`
 *
 * Binds to caller-owned storage via a raw pointer (not a reference — mirrors
 * `CheckboxWidget`'s `bool*` reasoning). Produces the exact same
 * `ImGui::ColorEdit3()`/`ColorEdit4()` call as `ColorEdit::Show()`.
 *
 * @since    2.5.0
 *
 * @example
 * @code
 * Vec4 tint{1.0f, 0.5f, 0.0f, 1.0f};
 * ColorEditWidget("Tint", &tint).Alpha(true).OnChange([&](Vec4 c) { material.SetTint(c); });
 * @endcode
 */
class ColorEditWidget {
public:
    /// `value` must outlive this widget and every `Element` mounted from it.
    ColorEditWidget(std::string label, Vec4* value) : _label(std::move(label)), _value(value) {}

    ColorEditWidget& Alpha(bool alpha = true) noexcept { _alpha = alpha; return *this; }
    ColorEditWidget& OnChange(Utility::Delegate<void(Vec4)> cb) { _onChange = std::move(cb); return *this; }
    ColorEditWidget& Disabled(bool disabled = true) noexcept { _disabled = disabled; return *this; }
    ColorEditWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }
    ColorEditWidget& Width(float w) noexcept { _width = w; return *this; }

    /// Explicit identity override — see `Tree::Key`.
    ColorEditWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetLabel() const noexcept { return _label; }
    [[nodiscard]] Vec4* GetValue() const noexcept { return _value; }
    [[nodiscard]] bool GetAlpha() const noexcept { return _alpha; }
    [[nodiscard]] const Utility::Delegate<void(Vec4)>& GetOnChange() const noexcept { return _onChange; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }
    [[nodiscard]] float GetWidth() const noexcept { return _width; }

    /// @internal Produces this colour editor's concrete `Element`. Defined in `ColorEdit.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    std::string                    _label;
    Vec4*                          _value = nullptr;
    Utility::Delegate<void(Vec4)>  _onChange;
    bool                           _alpha    = false;
    bool                           _disabled = false;
    std::string                    _tooltip;
    float                          _width    = 0.0f;
    Tree::Key                      _key;
};

} // namespace ImFrame::Widgets
