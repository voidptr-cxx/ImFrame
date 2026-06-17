/**
 * @file     InputTranslation.hpp
 * @brief    SDL3 event to ImFrame InputEvent translation utilities
 *
 * Pure functions — no SDL3 initialisation required. Tests create SDL_Event
 * structures directly and call these functions without a running SDL context.
 *
 * @internal
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-16
 * @version  2.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Backends/InputEvent.hpp"

#include <optional>
#include <SDL3/SDL.h>

namespace ImFrame::Internal::InputTranslation {

/**
 * @brief   Translate an SDL3 modifier state to ImFrame `ModFlags`.
 *
 * @param[in]  sdlMods  SDL_Keymod bitmask from an SDL event.
 * @return  Corresponding `ModFlags` value.
 * @throws  Nothing.
 */
[[nodiscard]] ModFlags TranslateModifiers(SDL_Keymod sdlMods) noexcept;

/**
 * @brief   Translate an SDL3 keyboard event to a `KeyEvent`.
 *
 * @param[in]  e  SDL_KeyboardEvent (from SDL_EVENT_KEY_DOWN or SDL_EVENT_KEY_UP).
 * @return  Populated `KeyEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] KeyEvent TranslateKey(const SDL_KeyboardEvent& e) noexcept;

/**
 * @brief   Translate an SDL3 text-input event to a `TextInputEvent`.
 *
 * Decodes the first UTF-8 codepoint from `SDL_TextInputEvent::text`.
 *
 * @param[in]  e  SDL_TextInputEvent (from SDL_EVENT_TEXT_INPUT).
 * @return  `TextInputEvent` with the first codepoint, or `std::nullopt` on decode failure.
 * @throws  Nothing.
 */
[[nodiscard]] std::optional<TextInputEvent> TranslateTextInput(const SDL_TextInputEvent& e) noexcept;

/**
 * @brief   Translate an SDL3 mouse-button event to a `MouseButtonEvent`.
 *
 * SDL button indices (1-based, with SDL_BUTTON_LEFT=1, SDL_BUTTON_RIGHT=3,
 * SDL_BUTTON_MIDDLE=2) are remapped to ImFrame 0-based (0=left, 1=right, 2=middle).
 *
 * @param[in]  e  SDL_MouseButtonEvent.
 * @return  Populated `MouseButtonEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] MouseButtonEvent TranslateMouseButton(const SDL_MouseButtonEvent& e) noexcept;

/**
 * @brief   Translate an SDL3 mouse-motion event to a `MouseMoveEvent`.
 *
 * @param[in]  e  SDL_MouseMotionEvent.
 * @return  Populated `MouseMoveEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] MouseMoveEvent TranslateMouseMotion(const SDL_MouseMotionEvent& e) noexcept;

/**
 * @brief   Translate an SDL3 mouse-wheel event to a `MouseScrollEvent`.
 *
 * Uses `precise_x` / `precise_y` for smooth trackpad scrolling.
 * `IsPrecise` is true when the precise values differ from their integer floor.
 *
 * @param[in]  e  SDL_MouseWheelEvent.
 * @return  Populated `MouseScrollEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] MouseScrollEvent TranslateMouseWheel(const SDL_MouseWheelEvent& e) noexcept;

/**
 * @brief   Translate an SDL3 gamepad-axis event to a `GamepadEvent`.
 *
 * Normalises the axis value from SDL3's [-32768, 32767] to [-1, 1] and
 * applies a 0.1f dead zone clamp.
 *
 * @param[in]  e  SDL_GamepadAxisEvent.
 * @return  Populated `GamepadEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] GamepadEvent TranslateGamepadAxis(const SDL_GamepadAxisEvent& e) noexcept;

/**
 * @brief   Translate an SDL3 gamepad-button event to a `GamepadEvent`.
 *
 * @param[in]  e  SDL_GamepadButtonEvent.
 * @return  Populated `GamepadEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] GamepadEvent TranslateGamepadButton(const SDL_GamepadButtonEvent& e) noexcept;

/**
 * @brief   Translate an SDL3 finger (touch) event to a `TouchEvent`.
 *
 * @param[in]  e      SDL_TouchFingerEvent.
 * @param[in]  phase  Whether the contact began, moved, ended, or was cancelled.
 * @return  Populated `TouchEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] TouchEvent TranslateTouch(const SDL_TouchFingerEvent& e, TouchPhase phase) noexcept;

/**
 * @brief   Translate an SDL3 window-resize event to a `WindowResizeEvent`.
 *
 * @param[in]  e       SDL_WindowEvent (SDL_EVENT_WINDOW_RESIZED).
 * @param[in]  handle  ImFrame window handle for the resized window.
 * @return  Populated `WindowResizeEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] WindowResizeEvent TranslateWindowResize(const SDL_WindowEvent& e,
                                                      WindowHandle handle) noexcept;

/**
 * @brief   Translate an SDL3 window-focus event to a `WindowFocusEvent`.
 *
 * @param[in]  handle  ImFrame window handle.
 * @param[in]  gained  `true` if focus was gained; `false` if lost.
 * @return  Populated `WindowFocusEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] WindowFocusEvent TranslateWindowFocus(WindowHandle handle, bool gained) noexcept;

/**
 * @brief   Translate an SDL3 window-close event to a `WindowCloseRequestEvent`.
 *
 * @param[in]  handle  ImFrame window handle.
 * @return  Populated `WindowCloseRequestEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] WindowCloseRequestEvent TranslateWindowClose(WindowHandle handle) noexcept;

} // namespace ImFrame::Internal::InputTranslation
