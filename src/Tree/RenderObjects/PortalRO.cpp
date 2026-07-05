/**
 * @file     PortalRO.cpp
 * @brief    `Portal::CreateElement()` — the only non-inline symbol `Portal` needs
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "PortalRO.hpp"

namespace ImFrame::Tree {

std::unique_ptr<Element> Portal::CreateElement() const {
    return std::make_unique<Internal::PortalElement>();
}

} // namespace ImFrame::Tree
