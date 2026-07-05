/**
 * @file     VStack.hpp
 * @brief    Vertical layout container arranging children top-to-bottom
 *
 * `VStack::Render()` is a variadic template constrained by `Widgets::Renderable`.
 * The spacing `Dummy` call is delegated to a non-template internal helper so that
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

/** @brief  Emit an invisible `ImGui::Dummy({0, spacing})` for vertical gap. */
void VStackDummy(float spacing);

} // namespace ImFrame::Internal

namespace ImFrame::Layout {

/**
 * @class    VStack
 * @brief    Renders a variadic set of widgets in a vertical column
 *
 * Children are rendered top-to-bottom with a configurable pixel gap between
 * them emitted via `ImGui::Dummy`. The dummy does not affect layout width.
 *
 * @since    1.1.0
 *
 * @example
 * @code
 * VStack(8.0f).Render(
 *     Widgets::Text("Label"),
 *     Widgets::TextInput<std::string>("Name", name),
 *     Widgets::Button("Submit")
 * );
 * @endcode
 */
/// @deprecated Use `Tree::Primitives::Flex(Flex::Axis::Vertical)` instead. See `Docs/Migration_v1_to_v2.md`. Removed in Phase 30.
class [[deprecated("See Docs/Migration_v1_to_v2.md.")]] VStack {
public:
    /**
     * @brief    Construct a VStack with optional spacing between items.
     * @param[in]  spacing  Vertical gap in pixels between children. Default: 4.
     * @throws   Nothing.
     */
    explicit VStack(float spacing = 4.0f) noexcept : _spacing(spacing) {}

    /**
     * @brief    Render all `widgets` in a vertical column.
     * @tparam   Ts  Widget types; each must satisfy `Widgets::Renderable`.
     * @param[in,out]  widgets  Widgets to render; `Show()` is called on each in order.
     * @return   Nothing.
     * @throws   Nothing.
     */
    template <Widgets::Renderable... Ts>
    void Render(Ts&&... widgets) {
        bool first = true;
        auto doRender = [&](auto& w) {
            if (!first) Internal::VStackDummy(_spacing);
            first = false;
            w.Show();
        };
        (doRender(widgets), ...);
    }

private:
    float _spacing; ///< Vertical gap in pixels between children.
};

} // namespace ImFrame::Layout
