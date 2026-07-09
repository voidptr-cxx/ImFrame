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

namespace ImFrame::Widgets {

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
