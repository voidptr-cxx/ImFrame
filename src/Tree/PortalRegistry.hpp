/**
 * @file     PortalRegistry.hpp
 * @brief    Thread-local registry of `PortalElement`s mounted during the current Mount/Update pass
 *
 * `Reconciler::Show()` first runs `Mount()`/`Update()` for the entire tree, then
 * `Layout()`, then `Paint()`. `PortalElement` registers itself here on every
 * `Mount()`/`Update()` call so that once the main tree finishes painting,
 * `Reconciler::Show()` can drain this registry and call `RenderDeferred()` on
 * each portal in registration order — placing portal content last in the ImGui
 * draw list (renders on top).
 *
 * The registry is thread-local, matching the pattern established by
 * `Internal::g_stateRegistrar`/`g_currentBuildingElement` in `Context.hpp` — an
 * ambient channel for the recursive Mount/Update walk, without threading a
 * parameter through every `Element::Mount()`/`Update()` override.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <vector>

namespace ImFrame::Internal {

class PortalElement; // Full definition in RenderObjects/PortalRO.hpp.

/// Registers `elem` for deferred rendering this frame. No-op if already registered.
void RegisterPortal(PortalElement* elem);

/// Removes `elem` from the registry (called from `PortalElement::Unmount()`).
void UnregisterPortal(PortalElement* elem);

/// Returns all portals registered so far this frame, in registration order, and clears the registry.
[[nodiscard]] std::vector<PortalElement*> DrainPortals();

} // namespace ImFrame::Internal
