/**
 * @file     InputEvent.hpp
 * @brief    Input event types and WindowHandle for the ImFrame input pipeline
 *
 * All input from hardware (keyboard, mouse, gamepad, touch, stylus) and from
 * window-management events (resize, focus, close) is represented as an
 * `InputEvent` variant. Backends accumulate events in an internal queue each
 * frame; `IBackend::DrainInputEvents()` returns the accumulated events to the
 * application layer, which partitions them between ImGui and application handlers.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <cstdint>
#include <variant>

namespace ImFrame {

// ─── WindowHandle ─────────────────────────────────────────────────────────────

/**
 * @brief  Opaque integer handle identifying a window managed by IBackend.
 *
 * Handle 0 (`PrimaryWindow`) is always the primary window, which exists for
 * the entire application lifetime after Init() succeeds. Secondary window
 * handles are allocated sequentially starting from 1.
 *
 * @since  1.9.0
 */
using WindowHandle = std::uint32_t;

/// Handle representing the primary window — always valid after Init().
inline constexpr WindowHandle PrimaryWindow = 0;

// ─── Modifier flags ───────────────────────────────────────────────────────────

/**
 * @enum   ModFlags
 * @brief  Bitmask of keyboard modifier keys active at the time of an input event
 * @since  1.9.0
 */
enum class ModFlags : std::uint8_t {
    None  = 0x00, ///< No modifiers active.
    Shift = 0x01, ///< Shift key.
    Ctrl  = 0x02, ///< Ctrl (Control) key.
    Alt   = 0x04, ///< Alt (Option on macOS) key.
    Super = 0x08, ///< Super (Windows/Command) key.
};

/// Bitwise OR for ModFlags.
[[nodiscard]] constexpr ModFlags operator|(ModFlags a, ModFlags b) noexcept {
    return static_cast<ModFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

/// Compound OR for ModFlags.
constexpr ModFlags& operator|=(ModFlags& a, ModFlags b) noexcept {
    return a = a | b;
}

/// Test whether a modifier flag is set.
[[nodiscard]] constexpr bool HasFlag(ModFlags flags, ModFlags test) noexcept {
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(test)) != 0;
}

// ─── KeyAction ────────────────────────────────────────────────────────────────

/**
 * @enum   KeyAction
 * @brief  Key or button press state reported by KeyEvent and MouseButtonEvent
 * @since  1.9.0
 */
enum class KeyAction : std::uint8_t {
    Pressed,   ///< Transitioned from up to down.
    Released,  ///< Transitioned from down to up.
    Repeated,  ///< Key held; OS repeat event (keyboard only).
};

// ─── TouchPhase ───────────────────────────────────────────────────────────────

/**
 * @enum   TouchPhase
 * @brief  Phase of a touch contact reported by TouchEvent
 * @since  1.9.0
 */
enum class TouchPhase : std::uint8_t {
    Began,     ///< New contact began.
    Moved,     ///< Contact moved since last event.
    Ended,     ///< Contact lifted from the surface.
    Cancelled, ///< Contact cancelled (e.g. system gesture interrupted).
};

// ─── Event types ──────────────────────────────────────────────────────────────

/**
 * @struct KeyEvent
 * @brief  A keyboard key was pressed, released, or repeated
 * @since  1.9.0
 */
struct KeyEvent {
    int       KeyCode;  ///< Platform key code (GLFW_KEY_*, VK_*, …).
    int       ScanCode; ///< Hardware scan code, layout-independent.
    KeyAction Action;   ///< Pressed, released, or repeated.
    ModFlags  Mods;     ///< Active modifiers at event time.
};

/**
 * @struct MouseButtonEvent
 * @brief  A mouse button was pressed or released
 * @since  1.9.0
 */
struct MouseButtonEvent {
    int       Button; ///< Button index: 0=left, 1=right, 2=middle.
    KeyAction Action; ///< Pressed or released (never Repeated).
    ModFlags  Mods;   ///< Active modifiers at event time.
    float     X;      ///< Cursor X at event time, window-relative pixels.
    float     Y;      ///< Cursor Y at event time, window-relative pixels.
};

/**
 * @struct MouseMoveEvent
 * @brief  The mouse cursor position changed
 * @since  1.9.0
 */
struct MouseMoveEvent {
    float X;      ///< Absolute cursor X, window-relative pixels.
    float Y;      ///< Absolute cursor Y, window-relative pixels.
    float DeltaX; ///< Change in X since the previous MouseMoveEvent.
    float DeltaY; ///< Change in Y since the previous MouseMoveEvent.
};

/**
 * @struct MouseScrollEvent
 * @brief  The mouse wheel scrolled or a trackpad performed a scroll gesture
 * @since  1.9.0
 */
struct MouseScrollEvent {
    float XOffset;   ///< Horizontal scroll delta (positive = right).
    float YOffset;   ///< Vertical scroll delta (positive = up).
    bool  IsPrecise; ///< True for trackpad smooth scroll; false for wheel clicks.
};

/**
 * @struct TextInputEvent
 * @brief  A printable character was typed (separate from the raw KeyEvent)
 * @since  1.9.0
 */
struct TextInputEvent {
    char32_t Codepoint; ///< UTF-32 Unicode codepoint of the typed character.
};

/**
 * @struct GamepadEvent
 * @brief  A gamepad axis changed value or a button changed state
 *
 * Axis values are normalised to [-1, 1] with dead-zone clamping already
 * applied. Only changes from the previous frame are emitted by the backend.
 *
 * @since  1.9.0
 */
struct GamepadEvent {
    int   DeviceIndex;   ///< Joystick/gamepad device index (0-based).
    bool  IsAxis;        ///< True = axis event; false = button event.
    int   Index;         ///< Axis or button index.
    float AxisValue;     ///< Normalised axis value [-1, 1]; valid when IsAxis.
    bool  ButtonPressed; ///< Button pressed state; valid when !IsAxis.
};

/**
 * @struct TouchEvent
 * @brief  A touch contact began, moved, ended, or was cancelled
 * @since  1.9.0
 */
struct TouchEvent {
    std::uint64_t ContactId;   ///< Unique contact identifier, stable over a contact lifetime.
    TouchPhase    Phase;
    float         X;           ///< Normalised X position [0, 1] within the window.
    float         Y;           ///< Normalised Y position [0, 1] within the window.
    float         MajorRadius; ///< Contact ellipse major-axis radius in pixels.
    float         MinorRadius; ///< Contact ellipse minor-axis radius in pixels.
    float         Force;       ///< Normalised contact force [0, 1].
};

/**
 * @struct StylusEvent
 * @brief  A stylus/pen moved or changed pressure or tilt
 * @since  1.9.0
 */
struct StylusEvent {
    float X;        ///< Cursor X, window-relative pixels.
    float Y;        ///< Cursor Y, window-relative pixels.
    float Pressure; ///< Normalised tip pressure [0, 1].
    float TiltX;    ///< Tilt around the X axis in radians.
    float TiltY;    ///< Tilt around the Y axis in radians.
    bool  IsEraser; ///< True when the eraser end is the active tip.
};

/**
 * @struct WindowResizeEvent
 * @brief  A window managed by IBackend was resized
 * @since  1.9.0
 */
struct WindowResizeEvent {
    WindowHandle Handle; ///< Window whose client area changed size.
    int          Width;  ///< New width in pixels.
    int          Height; ///< New height in pixels.
};

/**
 * @struct WindowFocusEvent
 * @brief  A window managed by IBackend gained or lost input focus
 * @since  1.9.0
 */
struct WindowFocusEvent {
    WindowHandle Handle; ///< Window whose focus state changed.
    bool         Gained; ///< True = gained focus; false = lost focus.
};

/**
 * @struct WindowCloseRequestEvent
 * @brief  The user requested to close a window managed by IBackend
 *
 * Returning from the current frame without calling `IBackend::DestroyWindow()`
 * keeps the window alive. The application layer decides whether to honour the
 * request or show a "save changes?" prompt.
 *
 * @since  1.9.0
 */
struct WindowCloseRequestEvent {
    WindowHandle Handle; ///< Window for which close was requested.
};

// ─── InputEvent variant ───────────────────────────────────────────────────────

/**
 * @brief  Discriminated union of all ImFrame input event types
 *
 * Use `std::visit` or `std::get_if` to handle specific event kinds:
 * @code
 * for (const auto& ev : backend.DrainInputEvents()) {
 *     std::visit([](auto&& e) {
 *         using T = std::decay_t<decltype(e)>;
 *         if constexpr (std::is_same_v<T, ImFrame::KeyEvent>) {
 *             // handle key
 *         }
 *     }, ev);
 * }
 * @endcode
 *
 * @since  1.9.0
 */
using InputEvent = std::variant<
    KeyEvent,
    MouseButtonEvent,
    MouseMoveEvent,
    MouseScrollEvent,
    TextInputEvent,
    GamepadEvent,
    TouchEvent,
    StylusEvent,
    WindowResizeEvent,
    WindowFocusEvent,
    WindowCloseRequestEvent
>;

} // namespace ImFrame
