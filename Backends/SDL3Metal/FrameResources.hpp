/**
 * @file     FrameResources.hpp
 * @brief    Per-frame-in-flight Metal resources reserved for Phase 24 Viewport use
 *
 * Metal's CPU/GPU synchronisation is a single `dispatch_semaphore_t` per window
 * (initial count == frames-in-flight) — not one semaphore per slot, unlike
 * Vulkan's per-slot fence. That semaphore lives on `SDL3MetalBackend::WindowData`
 * since it gates CPU submission rate for the window as a whole, not a specific
 * slot's GPU completion. `FrameResources` holds only the per-slot uniform buffer
 * that ImGui-only rendering does not use today but Phase 24's `Viewport` will.
 *
 * All members are ARC-managed strong references — no manual retain/release.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-18
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <Metal/Metal.h>

namespace ImFrame::Internal {

/**
 * @struct FrameResources
 * @brief  Metal objects owned by a single frame-in-flight slot for one window
 *
 * @since 2.1.0
 */
struct FrameResources {
    /// Reserved for Phase 24 Viewport per-frame uniform data. Unused by
    /// ImGui-only rendering. ARC releases it automatically with the slot.
    id<MTLBuffer> uniformBuffer = nil;
};

} // namespace ImFrame::Internal
