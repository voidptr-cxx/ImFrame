/**
 * @file     ClipStack.hpp
 * @brief    Tracks nested `PushClipRect`/`PopClipRect` state as an intersected effective clip region
 *
 * @internal
 * Mirrors ImGui's own clip-rect stack, but as a renderer-agnostic CPU data
 * structure: each `Push()` intersects the incoming rectangle with whatever is
 * currently on top, so `Current()` always reflects the *effective* visible
 * region, not just the most recently pushed one. `CurrentKind()` reports
 * whether any rectangle in the active chain has a non-zero corner radius —
 * per `PHASE_32_PROPOSAL.md`'s Clip Stack section, a future
 * `NativeRenderer` backend uses hardware scissor for `AxisAligned` regions
 * and a stencil mask pass for `Rounded` ones. This type only tracks *which*
 * strategy applies; issuing the actual scissor/stencil GPU calls is a
 * backend's job, added once one exists.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Rendering/CommandBuffer.hpp"
#include "ImFrame/Widgets/Types.hpp"

#include <vector>

namespace ImFrame::Internal {

/// An effective (already-intersected) clip rectangle.
struct ClipRect {
    Widgets::Vec2 Position{};      ///< Top-left of the intersected region.
    Widgets::Vec2 Size{0.0f, 0.0f}; ///< `{0, 0}` means the region is empty (fully clipped away).
    float         CornerRadius = 0.0f; ///< The most recently pushed rect's own radius (for reference/debugging).
};

/// Which GPU clipping strategy the active clip chain needs.
enum class ClipKind : unsigned char {
    AxisAligned, ///< No rectangle in the active chain has rounded corners — hardware scissor suffices.
    Rounded,     ///< At least one rectangle in the active chain has rounded corners — needs a stencil mask pass.
};

/**
 * @class    ClipStack
 * @brief    A nested, auto-intersecting clip-rectangle stack
 *
 * @since    3.0.0
 *
 * @example
 * @code
 * ClipStack clips;
 * clips.Push(Rendering::PushClipRect{.Position = {0, 0}, .Size = {100, 100}});
 * clips.Push(Rendering::PushClipRect{.Position = {50, 50}, .Size = {100, 100}});
 * // clips.Current() is now {50, 50} sized {50, 50} — the intersection of both.
 * clips.Pop();
 * @endcode
 */
class ClipStack {
public:
    /// Intersects `cmd`'s rectangle with the current top (or treats it as the first region if the stack is empty).
    void Push(const Rendering::PushClipRect& cmd);

    /// Pops the most recently pushed region. Precondition: `!Empty()`.
    void Pop();

    /// Clears the stack back to empty.
    void Reset();

    [[nodiscard]] bool        Empty() const noexcept { return _entries.empty(); }
    [[nodiscard]] std::size_t Depth() const noexcept { return _entries.size(); }

    /// The current effective (intersected) clip region. Precondition: `!Empty()`.
    [[nodiscard]] const ClipRect& Current() const noexcept;

    /// `ClipKind::AxisAligned` when the stack is empty or no active rectangle has rounded corners.
    [[nodiscard]] ClipKind CurrentKind() const noexcept;

private:
    struct Entry {
        ClipRect Effective;
        bool     HasRounded = false; ///< OR of this rect's own roundedness and every ancestor's.
    };

    std::vector<Entry> _entries;
};

} // namespace ImFrame::Internal
