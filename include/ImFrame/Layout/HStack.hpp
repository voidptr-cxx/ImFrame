/**
 * @file     HStack.hpp
 * @brief    Horizontal layout container arranging children left-to-right
 *
 * `HStack::Render()` is a variadic template constrained by `Widgets::Renderable`.
 * ImGui's `SameLine` call is delegated to a non-template internal helper so that
 * `<imgui.h>` is not pulled into this public header.
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

#include "ImFrame/Widgets/Types.hpp"

// ─── Internal helper (declared here for template Render() — not public API) ──
namespace ImFrame::Internal {

/** @brief  Call `ImGui::SameLine(0, spacing)`. Negative spacing uses the default item spacing. */
void HStackSameLine(float spacing);

} // namespace ImFrame::Internal

namespace ImFrame::Layout {

/**
 * @class    HStack
 * @brief    Renders a variadic set of widgets in a horizontal row
 *
 * Children are rendered left-to-right with `ImGui::SameLine()` inserted between
 * them. Spacing of `-1` (the default) uses ImGui's current item spacing.
 *
 * @since    1.1.0
 *
 * @example
 * @code
 * HStack(8.0f).Render(
 *     Widgets::Button("Cancel"),
 *     Widgets::Button("OK")
 * );
 * @endcode
 */
class HStack {
public:
    /**
     * @brief    Construct an HStack with optional explicit spacing.
     * @param[in]  spacing  Pixel gap between items, or `-1` for ImGui default.
     * @throws   Nothing.
     */
    explicit HStack(float spacing = -1.0f) noexcept : _spacing(spacing) {}

    /**
     * @brief    Render all `widgets` in a horizontal row.
     * @tparam   Ts  Widget types; each must satisfy `Widgets::Renderable`.
     * @param[in,out]  widgets  Widgets to render; `Show()` is called on each in order.
     * @return   Nothing.
     * @throws   Nothing.
     */
    template <Widgets::Renderable... Ts>
    void Render(Ts&&... widgets) {
        bool first = true;
        auto doRender = [&](auto& w) {
            if (!first) Internal::HStackSameLine(_spacing);
            first = false;
            w.Show();
        };
        (doRender(widgets), ...);
    }

private:
    float _spacing; ///< Pixel gap between items; `-1` defers to ImGui default.
};

} // namespace ImFrame::Layout
