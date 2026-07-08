/**
 * @file     Separator.hpp
 * @brief    Horizontal visual separator line, optionally with a centred label
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
#include "ImFrame/Widgets/Types.hpp"

#include <string>
#include <string_view>

namespace ImFrame::Widgets {

/**
 * @class    Separator
 * @brief    Horizontal line separator; labelled via `ImGui::SeparatorText()` when text is set
 *
 * `Show()` always returns `false` — separators are not interactive.
 *
 * @deprecated Use `SeparatorWidget` instead (Phase 30). See `Docs/Migration_v1_to_v2.md`.
 *             Removed in Phase 30.2.
 *
 * @since    1.0.0
 *
 * @example
 * @code
 * Widgets::Separator().Show();                    // plain line
 * Widgets::Separator().Label("Advanced").Show();  // labelled section divider
 * @endcode
 */
class [[deprecated("Use SeparatorWidget instead. See Docs/Migration_v1_to_v2.md.")]] Separator {
public:
    Separator() = default;

    /**
     * @brief    Display a label centred in the separator via `ImGui::SeparatorText()`.
     * @param[in]  text  Label text. Empty string = plain separator line.
     * @return   `*this` for chaining.
     */
    Separator& Label(std::string_view text) { _label = text; return *this; }

    Separator& Disabled(bool d = true)  { _disabled = d;   return *this; }
    Separator& Tooltip(std::string_view tip) { _tooltip = tip; return *this; }
    Separator& Width(float w)           { _width = w;       return *this; }
    Separator& Id(std::string_view id)  { _id = id;         return *this; }

    /**
     * @brief    Render the separator. Always returns `false`.
     * @return   `false` — separators are not interactive.
     * @throws   Nothing.
     */
    bool Show();

private:
    std::string_view    _label;
    std::string_view    _tooltip;
    std::string_view    _id;
    float               _width    = 0.0f;
    bool                _disabled = false;
};

// ─── SeparatorWidget (Phase 30) ─────────────────────────────────────────────────

/**
 * @class    SeparatorWidget
 * @brief    Declarative separator line — `Tree::PrimitiveWidget` replacement for `Separator`
 *
 * Produces the exact same `ImGui::SeparatorText()`/`ImGui::Separator()` call as
 * `Separator::Show()`. Stateless — no caller-owned binding needed.
 *
 * @since    2.5.0
 *
 * @example
 * @code
 * SeparatorWidget();                       // plain line
 * SeparatorWidget().Label("Advanced");     // labelled section divider
 * @endcode
 */
class SeparatorWidget {
public:
    SeparatorWidget() = default;

    SeparatorWidget& Label(std::string label) { _label = std::move(label); return *this; }
    SeparatorWidget& Disabled(bool d = true) noexcept { _disabled = d; return *this; }
    SeparatorWidget& Tooltip(std::string tip) { _tooltip = std::move(tip); return *this; }

    /// Explicit identity override — see `Tree::Key`.
    SeparatorWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] const std::string& GetLabel() const noexcept { return _label; }
    [[nodiscard]] bool GetDisabled() const noexcept { return _disabled; }
    [[nodiscard]] const std::string& GetTooltip() const noexcept { return _tooltip; }

    /// @internal Produces this separator's concrete `Element`. Defined in `Separator.cpp`.
    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const;

private:
    std::string _label;
    bool        _disabled = false;
    std::string _tooltip;
    Tree::Key   _key;
};

} // namespace ImFrame::Widgets
