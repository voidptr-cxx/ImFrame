/**
 * @file     WidgetHelpers.hpp
 * @brief    Internal label-building utilities shared by all widget translation units
 *
 * @internal
 * Not part of the public API. Include only from `src/Widgets/`.
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

#include <cstdio>
#include <string_view>

namespace ImFrame::Internal {

/**
 * @brief  Write `"label##id"` (or just `"label"`) into a stack buffer via snprintf.
 *
 * Keeps ImGui item IDs unique without heap allocation. `buf` must be at least
 * `size` bytes. The result is always null-terminated; oversized labels are
 * silently truncated.
 */
inline void BuildLabelBuf(char* buf, std::size_t size,
                           std::string_view label, std::string_view id) {
    if (id.empty()) {
        std::snprintf(buf, size, "%.*s",
                      static_cast<int>(label.size()), label.data());
    } else {
        std::snprintf(buf, size, "%.*s##%.*s",
                      static_cast<int>(label.size()), label.data(),
                      static_cast<int>(id.size()), id.data());
    }
}

} // namespace ImFrame::Internal
