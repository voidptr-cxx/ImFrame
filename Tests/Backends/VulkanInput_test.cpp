/**
 * @file     VulkanInput_test.cpp
 * @brief    Unit tests for SDL3→InputEvent translation (no GPU required)
 *
 * Tests call InputTranslation pure functions directly with hand-crafted
 * SDL3 event structs. No SDL3 Init() or GPU is needed.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-16
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/SDL3Vulkan/InputTranslation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace ImFrame;
using namespace ImFrame::Internal::InputTranslation;

// ─── TranslateModifiers ───────────────────────────────────────────────────────

TEST_CASE("TranslateModifiers - no modifiers", "[unit]")
{
    ModFlags f = TranslateModifiers(SDL_KMOD_NONE);
    REQUIRE(f == ModFlags::None);
}

TEST_CASE("TranslateModifiers - all modifiers", "[unit]")
{
    SDL_Keymod sdlMods = SDL_KMOD_LSHIFT | SDL_KMOD_LCTRL | SDL_KMOD_LALT | SDL_KMOD_LGUI;
    ModFlags f = TranslateModifiers(sdlMods);
    REQUIRE(HasFlag(f, ModFlags::Shift));
    REQUIRE(HasFlag(f, ModFlags::Ctrl));
    REQUIRE(HasFlag(f, ModFlags::Alt));
    REQUIRE(HasFlag(f, ModFlags::Super));
}

TEST_CASE("TranslateModifiers - right-side modifiers", "[unit]")
{
    SDL_Keymod sdlMods = SDL_KMOD_RSHIFT | SDL_KMOD_RCTRL;
    ModFlags f = TranslateModifiers(sdlMods);
    REQUIRE(HasFlag(f, ModFlags::Shift));
    REQUIRE(HasFlag(f, ModFlags::Ctrl));
    REQUIRE(!HasFlag(f, ModFlags::Alt));
    REQUIRE(!HasFlag(f, ModFlags::Super));
}

// ─── TranslateKey ────────────────────────────────────────────────────────────

TEST_CASE("TranslateKey - key down", "[unit]")
{
    SDL_KeyboardEvent e{};
    e.down     = true;
    e.repeat   = false;
    e.key      = SDLK_SPACE;
    e.scancode = SDL_SCANCODE_SPACE;
    e.mod      = SDL_KMOD_NONE;

    KeyEvent k = TranslateKey(e);
    REQUIRE(k.KeyCode  == static_cast<int>(SDLK_SPACE));
    REQUIRE(k.ScanCode == static_cast<int>(SDL_SCANCODE_SPACE));
    REQUIRE(k.Action   == KeyAction::Pressed);
    REQUIRE(k.Mods     == ModFlags::None);
}

TEST_CASE("TranslateKey - key up", "[unit]")
{
    SDL_KeyboardEvent e{};
    e.down     = false;
    e.repeat   = false;
    e.key      = SDLK_RETURN;
    e.scancode = SDL_SCANCODE_RETURN;
    e.mod      = SDL_KMOD_NONE;

    KeyEvent k = TranslateKey(e);
    REQUIRE(k.Action == KeyAction::Released);
}

TEST_CASE("TranslateKey - key repeat", "[unit]")
{
    SDL_KeyboardEvent e{};
    e.down   = true;
    e.repeat = true;
    e.key    = SDLK_A;

    KeyEvent k = TranslateKey(e);
    REQUIRE(k.Action == KeyAction::Repeated);
}

TEST_CASE("TranslateKey - shift modifier", "[unit]")
{
    SDL_KeyboardEvent e{};
    e.down   = true;
    e.mod    = SDL_KMOD_LSHIFT;

    KeyEvent k = TranslateKey(e);
    REQUIRE(HasFlag(k.Mods, ModFlags::Shift));
}

// ─── TranslateTextInput ───────────────────────────────────────────────────────

TEST_CASE("TranslateTextInput - ASCII character", "[unit]")
{
    SDL_TextInputEvent e{};
    e.text = "A";

    auto opt = TranslateTextInput(e);
    REQUIRE(opt.has_value());
    REQUIRE(opt->Codepoint == U'A');
}

TEST_CASE("TranslateTextInput - empty string returns nullopt", "[unit]")
{
    SDL_TextInputEvent e{};
    e.text = "";

    auto opt = TranslateTextInput(e);
    REQUIRE(!opt.has_value());
}

TEST_CASE("TranslateTextInput - 2-byte UTF-8 (U+00E9)", "[unit]")
{
    SDL_TextInputEvent e{};
    // U+00E9 → 0xC3 0xA9
    static constexpr char utf8E9[] = { static_cast<char>(0xC3), static_cast<char>(0xA9), '\0' };
    e.text = utf8E9;

    auto opt = TranslateTextInput(e);
    REQUIRE(opt.has_value());
    REQUIRE(opt->Codepoint == static_cast<char32_t>(0x00E9));
}

// ─── TranslateMouseButton ─────────────────────────────────────────────────────

TEST_CASE("TranslateMouseButton - left button down", "[unit]")
{
    SDL_MouseButtonEvent e{};
    e.button = SDL_BUTTON_LEFT;
    e.down   = true;
    e.x      = 100.0f;
    e.y      = 200.0f;

    MouseButtonEvent mb = TranslateMouseButton(e);
    REQUIRE(mb.Button == 0);
    REQUIRE(mb.Action == KeyAction::Pressed);
    REQUIRE(mb.X      == Catch::Approx(100.0f));
    REQUIRE(mb.Y      == Catch::Approx(200.0f));
}

TEST_CASE("TranslateMouseButton - right button maps to index 1", "[unit]")
{
    SDL_MouseButtonEvent e{};
    e.button = SDL_BUTTON_RIGHT;
    e.down   = false;

    MouseButtonEvent mb = TranslateMouseButton(e);
    REQUIRE(mb.Button == 1);
    REQUIRE(mb.Action == KeyAction::Released);
}

TEST_CASE("TranslateMouseButton - middle button maps to index 2", "[unit]")
{
    SDL_MouseButtonEvent e{};
    e.button = SDL_BUTTON_MIDDLE;
    e.down   = true;

    MouseButtonEvent mb = TranslateMouseButton(e);
    REQUIRE(mb.Button == 2);
}

// ─── TranslateMouseMotion ─────────────────────────────────────────────────────

TEST_CASE("TranslateMouseMotion - position and delta", "[unit]")
{
    SDL_MouseMotionEvent e{};
    e.x    = 320.0f;
    e.y    = 240.0f;
    e.xrel = 5.0f;
    e.yrel = -3.0f;

    MouseMoveEvent mm = TranslateMouseMotion(e);
    REQUIRE(mm.X      == Catch::Approx(320.0f));
    REQUIRE(mm.Y      == Catch::Approx(240.0f));
    REQUIRE(mm.DeltaX == Catch::Approx(5.0f));
    REQUIRE(mm.DeltaY == Catch::Approx(-3.0f));
}

// ─── TranslateMouseWheel ──────────────────────────────────────────────────────

TEST_CASE("TranslateMouseWheel - integer wheel click", "[unit]")
{
    SDL_MouseWheelEvent e{};
    e.x         = 0.0f;
    e.y         = 1.0f;
    e.direction = SDL_MOUSEWHEEL_NORMAL;

    MouseScrollEvent ms = TranslateMouseWheel(e);
    REQUIRE(ms.YOffset == Catch::Approx(1.0f));
    REQUIRE(!ms.IsPrecise);
}

TEST_CASE("TranslateMouseWheel - precise trackpad scroll detected", "[unit]")
{
    SDL_MouseWheelEvent e{};
    e.x         = 0.0f;
    e.y         = 0.4f; // fractional → trackpad
    e.direction = SDL_MOUSEWHEEL_NORMAL;

    MouseScrollEvent ms = TranslateMouseWheel(e);
    REQUIRE(ms.IsPrecise);
}

TEST_CASE("TranslateMouseWheel - flipped direction inverts offsets", "[unit]")
{
    SDL_MouseWheelEvent e{};
    e.x         = 0.0f;
    e.y         = 2.0f;
    e.direction = SDL_MOUSEWHEEL_FLIPPED;

    MouseScrollEvent ms = TranslateMouseWheel(e);
    REQUIRE(ms.YOffset == Catch::Approx(-2.0f));
}

// ─── TranslateGamepadAxis ─────────────────────────────────────────────────────

TEST_CASE("TranslateGamepadAxis - positive full deflection", "[unit]")
{
    SDL_GamepadAxisEvent e{};
    e.which = 0;
    e.axis  = SDL_GAMEPAD_AXIS_LEFTX;
    e.value = 32767;

    GamepadEvent ge = TranslateGamepadAxis(e);
    REQUIRE(ge.IsAxis);
    REQUIRE(ge.AxisValue == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("TranslateGamepadAxis - value within dead zone clamped to zero", "[unit]")
{
    SDL_GamepadAxisEvent e{};
    e.which = 0;
    e.axis  = SDL_GAMEPAD_AXIS_LEFTX;
    e.value = 2000; // ~0.061f, within the 0.1f dead zone

    GamepadEvent ge = TranslateGamepadAxis(e);
    REQUIRE(ge.AxisValue == Catch::Approx(0.0f));
}

TEST_CASE("TranslateGamepadAxis - negative deflection outside dead zone", "[unit]")
{
    SDL_GamepadAxisEvent e{};
    e.which = 0;
    e.axis  = SDL_GAMEPAD_AXIS_LEFTY;
    e.value = -32768;

    GamepadEvent ge = TranslateGamepadAxis(e);
    REQUIRE(ge.AxisValue < -0.9f);
}

// ─── TranslateGamepadButton ───────────────────────────────────────────────────

TEST_CASE("TranslateGamepadButton - button down", "[unit]")
{
    SDL_GamepadButtonEvent e{};
    e.which  = 1;
    e.button = SDL_GAMEPAD_BUTTON_SOUTH;
    e.down   = true;

    GamepadEvent ge = TranslateGamepadButton(e);
    REQUIRE(!ge.IsAxis);
    REQUIRE(ge.ButtonPressed);
    REQUIRE(ge.DeviceIndex == 1);
}

// ─── TranslateWindowResize ────────────────────────────────────────────────────

TEST_CASE("TranslateWindowResize - carries handle and dimensions", "[unit]")
{
    SDL_WindowEvent e{};
    e.data1 = 1920;
    e.data2 = 1080;

    WindowResizeEvent wre = TranslateWindowResize(e, 42);
    REQUIRE(wre.Handle == 42);
    REQUIRE(wre.Width  == 1920);
    REQUIRE(wre.Height == 1080);
}

// ─── TranslateWindowFocus ─────────────────────────────────────────────────────

TEST_CASE("TranslateWindowFocus - gained", "[unit]")
{
    WindowFocusEvent wfe = TranslateWindowFocus(0, true);
    REQUIRE(wfe.Handle == 0);
    REQUIRE(wfe.Gained);
}

TEST_CASE("TranslateWindowFocus - lost", "[unit]")
{
    WindowFocusEvent wfe = TranslateWindowFocus(1, false);
    REQUIRE(!wfe.Gained);
}

// ─── TranslateWindowClose ─────────────────────────────────────────────────────

TEST_CASE("TranslateWindowClose - carries correct handle", "[unit]")
{
    WindowCloseRequestEvent wce = TranslateWindowClose(3);
    REQUIRE(wce.Handle == 3);
}
