/**
 * @file     iOSMetalBackend.mm
 * @brief    iOS Metal backend scaffold — intentionally does not compile
 *
 * @internal
 * Implementing this file requires a `CADisplayLink`-driven render loop (the
 * app does not own the main loop on iOS), `UIWindow`/`UIView` in place of
 * `NSWindow`/`NSView`, and UIKit-based input translation in place of SDL3's
 * event model — none of which exists yet. See the Phase 21 proposal's
 * "iOS Scaffold" section. This file exists so the CMake target structure and
 * header declarations are in place for whichever future phase implements iOS
 * support; it must not compile until that phase replaces this `#error` with
 * real method bodies.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-18
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#error "iOS Metal backend: not yet implemented — see Phase 21 scaffold"
