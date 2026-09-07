/**
 * @file     InputTranslation.cpp
 * @brief    SDL3 event to ImFrame InputEvent translation — pure function implementations
 *
 * @internal
 * None of these functions initialise SDL3 or require a running SDL context.
 * They operate only on the data fields of the SDL3 event unions.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-16
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "InputTranslation.hpp"

#include <cmath>
#include <cstring>

namespace ImFrame::Internal::InputTranslation {

namespace {

constexpr float GAMEPAD_DEAD_ZONE   = 0.1f;
constexpr float SDL_AXIS_NORMALISER = 32767.0f;

// Decode the first UTF-32 codepoint from a UTF-8 byte sequence.
char32_t DecodeFirstUtf8Codepoint(const char* utf8) noexcept
{
    if (!utf8 || utf8[0] == '\0') return 0;

    unsigned char c0 = static_cast<unsigned char>(utf8[0]);
    if (c0 < 0x80) return static_cast<char32_t>(c0);

    int extraBytes = 0;
    char32_t codepoint = 0;
    if ((c0 & 0xE0) == 0xC0)      { extraBytes = 1; codepoint = c0 & 0x1F; }
    else if ((c0 & 0xF0) == 0xE0) { extraBytes = 2; codepoint = c0 & 0x0F; }
    else if ((c0 & 0xF8) == 0xF0) { extraBytes = 3; codepoint = c0 & 0x07; }
    else return 0; // invalid lead byte

    for (int i = 1; i <= extraBytes; ++i) {
        unsigned char ci = static_cast<unsigned char>(utf8[i]);
        if (ci == '\0' || (ci & 0xC0) != 0x80) return 0;
        codepoint = (codepoint << 6) | (ci & 0x3F);
    }
    return codepoint;
}

} // anonymous namespace

// ─── TranslateModifiers ───────────────────────────────────────────────────────

ModFlags TranslateModifiers(SDL_Keymod sdlMods) noexcept
{
    ModFlags f = ModFlags::None;
    if (sdlMods & SDL_KMOD_SHIFT) f |= ModFlags::Shift;
    if (sdlMods & SDL_KMOD_CTRL)  f |= ModFlags::Ctrl;
    if (sdlMods & SDL_KMOD_ALT)   f |= ModFlags::Alt;
    if (sdlMods & SDL_KMOD_GUI)   f |= ModFlags::Super;
    return f;
}

// ─── TranslateKey ─────────────────────────────────────────────────────────────

KeyEvent TranslateKey(const SDL_KeyboardEvent& e) noexcept
{
    KeyAction action = KeyAction::Pressed;
    if (!e.down)         action = KeyAction::Released;
    else if (e.repeat)   action = KeyAction::Repeated;

    return KeyEvent{
        static_cast<int>(e.key),      // SDL_Keycode — layout-dependent
        static_cast<int>(e.scancode), // SDL_Scancode — hardware scan code
        action,
        TranslateModifiers(e.mod),
    };
}

// ─── TranslateTextInput ───────────────────────────────────────────────────────

std::optional<TextInputEvent> TranslateTextInput(const SDL_TextInputEvent& e) noexcept
{
    char32_t cp = DecodeFirstUtf8Codepoint(e.text);
    if (cp == 0) return std::nullopt;
    return TextInputEvent{ cp };
}

// ─── TranslateMouseButton ─────────────────────────────────────────────────────

MouseButtonEvent TranslateMouseButton(const SDL_MouseButtonEvent& e) noexcept
{
    // Remap SDL's 1-based button indices to ImFrame's 0-based (0=left, 1=right, 2=middle).
    int button;
    switch (e.button) {
        case SDL_BUTTON_LEFT:   button = 0; break;
        case SDL_BUTTON_RIGHT:  button = 1; break;
        case SDL_BUTTON_MIDDLE: button = 2; break;
        default: button = static_cast<int>(e.button) - 1; break;
    }
    return MouseButtonEvent{
        button,
        e.down ? KeyAction::Pressed : KeyAction::Released,
        TranslateModifiers(SDL_GetModState()),
        e.x,
        e.y,
    };
}

// ─── TranslateMouseMotion ─────────────────────────────────────────────────────

MouseMoveEvent TranslateMouseMotion(const SDL_MouseMotionEvent& e) noexcept
{
    return MouseMoveEvent{
        e.x,
        e.y,
        e.xrel,
        e.yrel,
    };
}

// ─── TranslateMouseWheel ──────────────────────────────────────────────────────

MouseScrollEvent TranslateMouseWheel(const SDL_MouseWheelEvent& e) noexcept
{
    // SDL3 provides precise floats for smooth trackpad scrolling.
    float px = e.x;
    float py = e.y;
    // Detect high-precision (trackpad) scroll: fractional part is non-zero.
    bool isPrecise = (std::fabsf(px - std::truncf(px)) > 0.001f) ||
                     (std::fabsf(py - std::truncf(py)) > 0.001f);
    // SDL_MOUSEWHEEL_FLIPPED inverts the sign on some platforms.
    if (e.direction == SDL_MOUSEWHEEL_FLIPPED) {
        px = -px;
        py = -py;
    }
    return MouseScrollEvent{ px, py, isPrecise };
}

// ─── TranslateGamepadAxis ─────────────────────────────────────────────────────

GamepadEvent TranslateGamepadAxis(const SDL_GamepadAxisEvent& e) noexcept
{
    float norm = static_cast<float>(e.value) / SDL_AXIS_NORMALISER;
    float val  = (std::fabsf(norm) < GAMEPAD_DEAD_ZONE) ? 0.0f : norm;
    return GamepadEvent{
        static_cast<int>(e.which),
        true,
        static_cast<int>(e.axis),
        val,
        false,
    };
}

// ─── TranslateGamepadButton ───────────────────────────────────────────────────

GamepadEvent TranslateGamepadButton(const SDL_GamepadButtonEvent& e) noexcept
{
    return GamepadEvent{
        static_cast<int>(e.which),
        false,
        static_cast<int>(e.button),
        0.0f,
        e.down,
    };
}

// ─── TranslateTouch ───────────────────────────────────────────────────────────

TouchEvent TranslateTouch(const SDL_TouchFingerEvent& e, TouchPhase phase) noexcept
{
    return TouchEvent{
        static_cast<uint64_t>(e.fingerID),
        phase,
        e.x,
        e.y,
        e.pressure, // SDL3 provides a single radius — use as both major and minor
        e.pressure,
        e.pressure,
    };
}

// ─── TranslateWindowResize ────────────────────────────────────────────────────

WindowResizeEvent TranslateWindowResize(const SDL_WindowEvent& e, WindowHandle handle) noexcept
{
    return WindowResizeEvent{
        handle,
        e.data1,
        e.data2,
    };
}

// ─── TranslateWindowFocus ─────────────────────────────────────────────────────

WindowFocusEvent TranslateWindowFocus(WindowHandle handle, bool gained) noexcept
{
    return WindowFocusEvent{ handle, gained };
}

// ─── TranslateWindowClose ─────────────────────────────────────────────────────

WindowCloseRequestEvent TranslateWindowClose(WindowHandle handle) noexcept
{
    return WindowCloseRequestEvent{ handle };
}

} // namespace ImFrame::Internal::InputTranslation
