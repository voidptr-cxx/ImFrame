/**
 * @file     ApplicationContext.hpp
 * @brief    Thread-local Application pointer used by Viewport::Show() to reach the registry
 *
 * The current Application sets itself as the active context at the start of
 * `RunOneFrame()` and clears it on destruction. Viewport::Show() calls
 * `GetCurrentApplication()` to find its owning Application without requiring
 * an explicit parameter.
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

namespace ImFrame::App { class Application; }

namespace ImFrame::Internal {

/// Return the Application that is currently executing RunOneFrame(), or nullptr.
[[nodiscard]] App::Application* GetCurrentApplication() noexcept;

/// Set (or clear) the current Application. Called by Application::RunOneFrame().
void SetCurrentApplication(App::Application* app) noexcept;

} // namespace ImFrame::Internal
