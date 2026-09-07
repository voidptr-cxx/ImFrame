/**
 * @file     HeadlessBackend.hpp
 * @brief    Forwarding include — implementation moved to Backends/Headless/ in Phase 19
 *
 * @internal
 * This file is kept for historical reference only. The full HeadlessBackend
 * implementation now lives in Backends/Headless/HeadlessBackend.hpp and is
 * compiled as part of the ImFrame static library.
 *
 * Application.cpp now includes Backends/Headless/HeadlessBackend.hpp directly.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-03
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

// ${CMAKE_SOURCE_DIR} is a PRIVATE include dir of ImFrame so this resolves.
#include "Backends/Headless/HeadlessBackend.hpp"
