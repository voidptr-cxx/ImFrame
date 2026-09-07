/**
 * @file     InputTranslationEmscripten.hpp
 * @brief    DOM event struct to ImFrame InputEvent translation utilities (Emscripten only)
 *
 * Pure functions — no Emscripten runtime required to call them, mirroring
 * `Backends/SDL3Vulkan/InputTranslation.hpp`'s testability contract. SDL3 is
 * not available on Emscripten (see the Phase 23 proposal's Input section), so
 * the backend registers `emscripten_set_*_callback` DOM listeners directly
 * and calls these functions from inside each callback.
 *
 * @internal
 * UNVERIFIED — written from `<emscripten/html5.h>`'s actual struct
 * definitions (verified against the installed emsdk 6.0.0) but never
 * compiled as part of an Emscripten ImFrame build. See PHASE_STATUS.md.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-22
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Backends/InputEvent.hpp"

#include <emscripten/html5.h>

namespace ImFrame::Internal::InputTranslationEmscripten {

/**
 * @brief   Translate a DOM keyboard event to a `KeyEvent`.
 *
 * `KeyCode`/`ScanCode` both carry the DOM `KeyboardEvent.keyCode` value —
 * there is no GLFW/VK-style platform key code on the web. A DOM
 * `key`-string-to-`ImGuiKey` table is the consuming application/Application
 * layer's responsibility when forwarding to ImGui (out of scope for this
 * pure translation function, matching every other backend's input module).
 *
 * @param[in]  e       Emscripten keyboard event.
 * @param[in]  action  Pressed, Released, or Repeated (Repeated comes from `e.repeat`).
 * @return  Populated `KeyEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] KeyEvent TranslateKey(const EmscriptenKeyboardEvent& e, KeyAction action) noexcept;

/**
 * @brief   Translate a DOM mouse button event to a `MouseButtonEvent`.
 *
 * DOM button indices (0=left, 1=middle, 2=right) are remapped to ImFrame's
 * convention (0=left, 1=right, 2=middle) — same remap target as SDL3's
 * `TranslateMouseButton`, different source mapping.
 *
 * @param[in]  e       Emscripten mouse event.
 * @param[in]  action  Pressed or Released.
 * @return  Populated `MouseButtonEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] MouseButtonEvent TranslateMouseButton(const EmscriptenMouseEvent& e, KeyAction action) noexcept;

/**
 * @brief   Translate a DOM mouse move event to a `MouseMoveEvent`.
 *
 * Uses `movementX`/`movementY` directly (the DOM's own pointer-delta field)
 * rather than tracking previous position — no per-call state needed.
 *
 * @param[in]  e  Emscripten mouse event.
 * @return  Populated `MouseMoveEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] MouseMoveEvent TranslateMouseMove(const EmscriptenMouseEvent& e) noexcept;

/**
 * @brief   Translate a DOM wheel event to a `MouseScrollEvent`.
 *
 * `deltaY` sign is negated — DOM convention is positive-down, ImFrame's
 * `YOffset` convention is positive-up (matching SDL3's `precise_y`).
 *
 * @param[in]  e  Emscripten wheel event.
 * @return  Populated `MouseScrollEvent`.
 * @throws  Nothing.
 */
[[nodiscard]] MouseScrollEvent TranslateMouseWheel(const EmscriptenWheelEvent& e) noexcept;

/**
 * @brief   Translate a single DOM touch point to a `TouchEvent`.
 *
 * @param[in]  point         One entry from `EmscriptenTouchEvent::touches`.
 * @param[in]  phase         Began, Moved, Ended, or Cancelled.
 * @param[in]  canvasWidth   Canvas CSS width in pixels, for normalising X to [0, 1].
 * @param[in]  canvasHeight  Canvas CSS height in pixels, for normalising Y to [0, 1].
 * @return  Populated `TouchEvent`. `Force`/radius fields are not available
 *          from `EmscriptenTouchPoint` and are left at zero.
 * @throws  Nothing.
 */
[[nodiscard]] TouchEvent TranslateTouchPoint(const EmscriptenTouchPoint& point, TouchPhase phase,
                                              double canvasWidth, double canvasHeight) noexcept;

/**
 * @brief   Translate a modifier bitmask from a DOM event's bool fields.
 *
 * @param[in]  ctrl   `ctrlKey`.
 * @param[in]  shift  `shiftKey`.
 * @param[in]  alt    `altKey`.
 * @param[in]  meta   `metaKey` — mapped to `ModFlags::Super`.
 * @return  Corresponding `ModFlags` value.
 * @throws  Nothing.
 */
[[nodiscard]] ModFlags TranslateModifiers(bool ctrl, bool shift, bool alt, bool meta) noexcept;

} // namespace ImFrame::Internal::InputTranslationEmscripten
