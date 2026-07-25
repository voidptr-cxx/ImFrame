/**
 * @file     IRenderer.hpp
 * @brief    Abstract interface for translating a `Rendering::CommandBuffer` into real draw calls
 *
 * @internal
 * `Application` owns a `std::unique_ptr<IRenderer>` — `ImGuiCompatRenderer` is
 * the default (and, until Phase 32, only) implementation. A future native
 * renderer (Phase 32) implements this same interface to turn the identical
 * `CommandBuffer` into GPU draw calls instead of ImGui draw-list calls.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-16
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Rendering/CommandBuffer.hpp"

namespace ImFrame::Internal {

/**
 * @class    IRenderer
 * @brief    Turns one frame's recorded `CommandBuffer` into real draw calls
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;

    /**
     * @brief    Replays every command in `buffer` as real draw calls.
     * @param[in] buffer  This frame's fully-recorded command buffer.
     */
    virtual void Render(const Rendering::CommandBuffer& buffer) = 0;

    /// Releases any renderer-owned resources. Safe to call multiple times.
    virtual void Shutdown() = 0;
};

} // namespace ImFrame::Internal
