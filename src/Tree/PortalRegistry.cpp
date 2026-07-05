/**
 * @file     PortalRegistry.cpp
 * @brief    Implementation of the thread-local portal registry
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

#include "PortalRegistry.hpp"

#include <algorithm>

namespace ImFrame::Internal {

namespace {
thread_local std::vector<PortalElement*> g_portalRegistry;
} // namespace

void RegisterPortal(PortalElement* elem) {
    if (std::find(g_portalRegistry.begin(), g_portalRegistry.end(), elem) == g_portalRegistry.end()) {
        g_portalRegistry.push_back(elem);
    }
}

void UnregisterPortal(PortalElement* elem) {
    std::erase(g_portalRegistry, elem);
}

std::vector<PortalElement*> DrainPortals() {
    std::vector<PortalElement*> result = std::move(g_portalRegistry);
    g_portalRegistry.clear();
    return result;
}

} // namespace ImFrame::Internal
