/**
 * @file     ChildScope.hpp
 * @brief    RAII scope wrapping ImGui::BeginChild / ImGui::EndChild
 *
 * `ChildScope` is a general-purpose `ImGui::BeginChild()`/`ImGui::EndChild()`
 * RAII guard. It guarantees that `ImGui::EndChild()` is called exactly once
 * per `BeginChild()` call, even when the child window is fully clipped. The
 * `Panel` and `ScrollArea` widgets that used to return it were removed as
 * deprecated in Phase 30.2 — the type is retained as a standalone utility for
 * any future or internal code that wraps `ImGui::BeginChild()` directly.
 *
 * Use it as an `if` condition: the body executes only when the child window is
 * visible; the destructor always calls `EndChild()` on scope exit.
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

namespace ImFrame::Layout {

/**
 * @class    ChildScope
 * @brief    RAII wrapper ensuring `ImGui::EndChild()` is called exactly once
 *
 * Moveable but not copyable. The moved-from scope is marked inactive and will
 * not call `EndChild()` in its destructor.
 *
 * @since    1.1.0
 *
 * @example
 * @code
 * if (auto scope = ChildScope(ImGui::BeginChild("sidebar"))) {
 *     ImGui::Text("Hello");
 * }
 * @endcode
 */
class ChildScope {
public:
    /**
     * @brief    Construct from the return value of `ImGui::BeginChild()`.
     * @param[in]  visible  `true` if the child window is visible and should be rendered.
     * @throws   Nothing.
     */
    explicit ChildScope(bool visible) noexcept : _visible(visible) {}

    /// Calls `ImGui::EndChild()` if this scope is still active.
    ~ChildScope() noexcept;

    ChildScope(const ChildScope&)            = delete;
    ChildScope& operator=(const ChildScope&) = delete;

    /** @brief   Transfer ownership; the moved-from scope becomes inactive. */
    ChildScope(ChildScope&& other) noexcept;
    ChildScope& operator=(ChildScope&&) = delete;

    /**
     * @brief    Convert to `bool` for use in `if` conditions.
     * @return   `true` if the child window is visible; `false` if clipped or collapsed.
     */
    [[nodiscard]] explicit operator bool() const noexcept { return _visible; }

private:
    bool _visible = false; ///< Visibility returned by `BeginChild()`.
    bool _active  = true;  ///< `false` after move — prevents double `EndChild()`.
};

} // namespace ImFrame::Layout
