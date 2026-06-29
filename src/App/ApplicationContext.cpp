/**
 * @file     ApplicationContext.cpp
 * @brief    Thread-local current-Application pointer for Viewport::Show()
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-27
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "App/ApplicationContext.hpp"

namespace ImFrame::Internal {

static App::Application* s_currentApp = nullptr;

App::Application* GetCurrentApplication() noexcept { return s_currentApp; }

void SetCurrentApplication(App::Application* app) noexcept { s_currentApp = app; }

} // namespace ImFrame::Internal
