/**
 * @file     ChildScope.cpp
 * @brief    RAII destructor and move constructor for ChildScope
 *
 * @internal
 * `ImGui::EndChild()` must be called exactly once for every `ImGui::BeginChild()`
 * call regardless of whether `BeginChild()` returned true (ImGui 1.90+ contract).
 * The non-inline destructor is defined here to confine `<imgui.h>` to this TU.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-07
 * @version  1.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Layout/ChildScope.hpp"

#include <imgui.h>

namespace ImFrame::Layout {

ChildScope::~ChildScope() {
    if (_active) {
        ImGui::EndChild();
    }
}

ChildScope::ChildScope(ChildScope&& other) noexcept
    : _visible(other._visible), _active(other._active) {
    other._active = false;
}

} // namespace ImFrame::Layout
