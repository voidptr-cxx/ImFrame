/**
 * @file     InputTranslationEmscripten.cpp
 * @brief    DOM event struct to ImFrame InputEvent translation — implementations
 *
 * @internal
 * UNVERIFIED — see InputTranslationEmscripten.hpp.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "InputTranslationEmscripten.hpp"

namespace ImFrame::Internal::InputTranslationEmscripten {

ModFlags TranslateModifiers(bool ctrl, bool shift, bool alt, bool meta) noexcept
{
    ModFlags mods = ModFlags::None;
    if (ctrl)  mods |= ModFlags::Ctrl;
    if (shift) mods |= ModFlags::Shift;
    if (alt)   mods |= ModFlags::Alt;
    if (meta)  mods |= ModFlags::Super;
    return mods;
}

KeyEvent TranslateKey(const EmscriptenKeyboardEvent& e, KeyAction action) noexcept
{
    return KeyEvent{
        .KeyCode  = static_cast<int>(e.keyCode),
        .ScanCode = static_cast<int>(e.keyCode),
        .Action   = e.repeat ? KeyAction::Repeated : action,
        .Mods     = TranslateModifiers(e.ctrlKey, e.shiftKey, e.altKey, e.metaKey),
    };
}

MouseButtonEvent TranslateMouseButton(const EmscriptenMouseEvent& e, KeyAction action) noexcept
{
    // DOM: 0=left, 1=middle, 2=right -> ImFrame: 0=left, 1=right, 2=middle.
    int button = 0;
    switch (e.button) {
        case 0:  button = 0; break; // left
        case 1:  button = 2; break; // middle
        case 2:  button = 1; break; // right
        default: button = static_cast<int>(e.button); break;
    }

    return MouseButtonEvent{
        .Button = button,
        .Action = action,
        .Mods   = TranslateModifiers(e.ctrlKey, e.shiftKey, e.altKey, e.metaKey),
        .X      = static_cast<float>(e.targetX),
        .Y      = static_cast<float>(e.targetY),
    };
}

MouseMoveEvent TranslateMouseMove(const EmscriptenMouseEvent& e) noexcept
{
    return MouseMoveEvent{
        .X      = static_cast<float>(e.targetX),
        .Y      = static_cast<float>(e.targetY),
        .DeltaX = static_cast<float>(e.movementX),
        .DeltaY = static_cast<float>(e.movementY),
    };
}

MouseScrollEvent TranslateMouseWheel(const EmscriptenWheelEvent& e) noexcept
{
    return MouseScrollEvent{
        .XOffset   = static_cast<float>(e.deltaX),
        .YOffset   = static_cast<float>(-e.deltaY), // DOM positive-down -> ImFrame positive-up.
        .IsPrecise = (e.deltaMode == DOM_DELTA_PIXEL),
    };
}

TouchEvent TranslateTouchPoint(const EmscriptenTouchPoint& point, TouchPhase phase, double canvasWidth,
                                double canvasHeight) noexcept
{
    float nx = (canvasWidth  > 0.0) ? static_cast<float>(point.targetX / canvasWidth)  : 0.0f;
    float ny = (canvasHeight > 0.0) ? static_cast<float>(point.targetY / canvasHeight) : 0.0f;

    return TouchEvent{
        .ContactId   = static_cast<std::uint64_t>(point.identifier),
        .Phase       = phase,
        .X           = nx,
        .Y           = ny,
        .MajorRadius = 0.0f, // Not available from EmscriptenTouchPoint.
        .MinorRadius = 0.0f,
        .Force       = 0.0f,
    };
}

} // namespace ImFrame::Internal::InputTranslationEmscripten
