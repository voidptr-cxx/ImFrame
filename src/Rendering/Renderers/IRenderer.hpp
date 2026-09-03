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
 * `LoadFont()` (Phase 33.8) is the generic seam `Application::WithFont()` calls
 * through, regardless of which concrete `IRenderer` is active — `Application`
 * itself never checks which one it is (see `.claude/DECISIONS.md`, Phase 33.8).
 * Each implementation answers differently: `ImGuiCompatRenderer::LoadFont()`
 * loads into ImGui's own font atlas (unchanged Phase 9 behaviour); a
 * `NativeRendererGL3` with an attached `ITextRenderer` forwards to its real
 * MSDF `FontRegistry`.
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

#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Rendering/CommandBuffer.hpp"
#include "ImFrame/Utility/Path.hpp"

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

    /**
     * @brief    Loads a font file for later use as a `Rendering::DrawText::Font`.
     *
     * @param[in]  path        Path to a `.ttf`/`.otf` file.
     * @param[in]  sizePixels  Initial pixel size hint — see the concrete implementation for
     *                         exactly how it's used.
     * @return   A `Rendering::FontId` usable as `DrawText::Font`, or an `Error` if this
     *           renderer's font pipeline is unavailable or the load itself failed.
     * @throws   Nothing.
     */
    [[nodiscard]] virtual Result<Rendering::FontId> LoadFont(const Utility::Path& path, float sizePixels) = 0;
};

} // namespace ImFrame::Internal
