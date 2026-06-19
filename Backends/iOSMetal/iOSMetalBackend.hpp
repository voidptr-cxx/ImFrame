/**
 * @file     iOSMetalBackend.hpp
 * @brief    iOS Metal backend scaffold — IBackend implementation (not yet functional)
 *
 * Structural scaffold only. iOS differs from macOS in three ways this class
 * must express: `UIWindow`/`UIView` replace `NSWindow`/`NSView`, `CADisplayLink`
 * drives the render loop instead of `SDL_PollEvent()` (the app does not own
 * the main loop on iOS), and SDL3 is not used at all — windowing and input go
 * through UIKit directly. `iOSMetalBackend` replaces `SDL3MetalBackend`
 * entirely on this platform; it does not extend or reuse it.
 *
 * Not implemented. See `iOSMetalBackend.mm`.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-18
 * @version  2.1.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/BackendInfo.hpp"

namespace ImFrame::Internal {

/**
 * @class    iOSMetalBackend
 * @brief    IBackend implementation for iOS using UIKit + Metal (scaffold only)
 *
 * @warning  Not implemented — a future phase drives the render loop via
 *           `CADisplayLink` and replaces SDL3's windowing/input layer with
 *           UIKit. Every method body lives in `iOSMetalBackend.mm`, which
 *           intentionally fails to compile (`#error`) until that phase lands.
 *
 * @since    2.1.0
 */
class iOSMetalBackend final : public IBackend {
public:
    iOSMetalBackend()           = default;
    ~iOSMetalBackend() override = default;

    VoidResult Init(const WindowConfig& config) override;
    FrameInfo  Poll() override;
    void       BeginFrame(WindowHandle handle = PrimaryWindow) override;
    void       EndFrame(WindowHandle handle = PrimaryWindow) override;
    void       Shutdown() override;
    void*      NativeHandle() const override;
    void       CancelClose() noexcept override;
};

} // namespace ImFrame::Internal
