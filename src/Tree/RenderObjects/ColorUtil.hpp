/**
 * @file     ColorUtil.hpp
 * @brief    Shared `Vec4` → `ImU32` conversion for RenderObject implementations
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Widgets/Types.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>

namespace ImFrame::Internal {

[[nodiscard]] inline ImU32 ToImU32(Widgets::Vec4 c) noexcept {
    auto channel = [](float v) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return IM_COL32(channel(c.x), channel(c.y), channel(c.z), channel(c.w));
}

} // namespace ImFrame::Internal
