/**
 * @file     InputEvent_test.cpp
 * @brief    Unit tests for the InputEvent variant and related types
 *
 * @internal
 * Tests verify:
 * - All InputEvent variant types construct correctly.
 * - std::visit over the variant compiles and executes for all alternatives.
 * - std::get_if correctly identifies variant alternatives.
 * - ModFlags bitwise operators work as expected.
 * - HeadlessBackend::InjectInputEvent / DrainInputEvents round-trip.
 *
 * No GPU or OS window is required. HeadlessBackend provides the inject/drain
 * mechanism used to verify the pipeline.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-15
 * @version  1.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/Backends/InputEvent.hpp"

#include <catch2/catch_test_macros.hpp>
#include <type_traits>

using namespace ImFrame;

// ─── Variant construction ─────────────────────────────────────────────────────

TEST_CASE("KeyEvent constructs and is held in InputEvent variant", "[unit]")
{
    InputEvent ev = KeyEvent{ 65, 30, KeyAction::Pressed, ModFlags::Ctrl };
    REQUIRE(std::holds_alternative<KeyEvent>(ev));

    const auto& ke = std::get<KeyEvent>(ev);
    REQUIRE(ke.KeyCode  == 65);
    REQUIRE(ke.ScanCode == 30);
    REQUIRE(ke.Action   == KeyAction::Pressed);
    REQUIRE(HasFlag(ke.Mods, ModFlags::Ctrl));
}

TEST_CASE("MouseButtonEvent constructs correctly", "[unit]")
{
    InputEvent ev = MouseButtonEvent{ 0, KeyAction::Released, ModFlags::None, 100.0f, 200.0f };
    REQUIRE(std::holds_alternative<MouseButtonEvent>(ev));
    REQUIRE(std::get<MouseButtonEvent>(ev).Button == 0);
}

TEST_CASE("MouseMoveEvent constructs correctly", "[unit]")
{
    InputEvent ev = MouseMoveEvent{ 10.0f, 20.0f, 1.0f, -1.0f };
    REQUIRE(std::holds_alternative<MouseMoveEvent>(ev));
    REQUIRE(std::get<MouseMoveEvent>(ev).DeltaX == 1.0f);
}

TEST_CASE("MouseScrollEvent IsPrecise flag is preserved", "[unit]")
{
    InputEvent ev = MouseScrollEvent{ 0.0f, -3.0f, true };
    REQUIRE(std::holds_alternative<MouseScrollEvent>(ev));
    REQUIRE(std::get<MouseScrollEvent>(ev).IsPrecise);
}

TEST_CASE("TextInputEvent codepoint is preserved", "[unit]")
{
    InputEvent ev = TextInputEvent{ U'é' }; // é
    REQUIRE(std::holds_alternative<TextInputEvent>(ev));
    REQUIRE(std::get<TextInputEvent>(ev).Codepoint == U'é');
}

TEST_CASE("GamepadEvent axis flag is preserved", "[unit]")
{
    InputEvent ev = GamepadEvent{ 0, true, 1, 0.75f, false };
    REQUIRE(std::holds_alternative<GamepadEvent>(ev));
    const auto& ge = std::get<GamepadEvent>(ev);
    REQUIRE(ge.IsAxis);
    REQUIRE(ge.AxisValue == 0.75f);
}

TEST_CASE("WindowResizeEvent carries handle and dimensions", "[unit]")
{
    InputEvent ev = WindowResizeEvent{ PrimaryWindow, 1920, 1080 };
    REQUIRE(std::holds_alternative<WindowResizeEvent>(ev));
    const auto& re = std::get<WindowResizeEvent>(ev);
    REQUIRE(re.Handle == PrimaryWindow);
    REQUIRE(re.Width  == 1920);
    REQUIRE(re.Height == 1080);
}

TEST_CASE("WindowFocusEvent carries gained flag", "[unit]")
{
    InputEvent ev = WindowFocusEvent{ PrimaryWindow, false };
    REQUIRE(std::holds_alternative<WindowFocusEvent>(ev));
    REQUIRE(!std::get<WindowFocusEvent>(ev).Gained);
}

TEST_CASE("WindowCloseRequestEvent carries window handle", "[unit]")
{
    InputEvent ev = WindowCloseRequestEvent{ 2u };
    REQUIRE(std::holds_alternative<WindowCloseRequestEvent>(ev));
    REQUIRE(std::get<WindowCloseRequestEvent>(ev).Handle == 2u);
}

// ─── std::visit over all alternatives ────────────────────────────────────────

TEST_CASE("std::visit executes for all InputEvent alternative types", "[unit]")
{
    using AllTypes = std::tuple<
        KeyEvent, MouseButtonEvent, MouseMoveEvent, MouseScrollEvent,
        TextInputEvent, GamepadEvent, TouchEvent, StylusEvent,
        WindowResizeEvent, WindowFocusEvent, WindowCloseRequestEvent
    >;

    static constexpr std::size_t N = std::tuple_size_v<AllTypes>;
    REQUIRE(std::variant_size_v<InputEvent> == N);

    int visitCount = 0;
    auto visitor = [&](auto&&) { ++visitCount; };

    // Construct one of each and visit it.
    InputEvent evs[] = {
        KeyEvent{},
        MouseButtonEvent{},
        MouseMoveEvent{},
        MouseScrollEvent{},
        TextInputEvent{},
        GamepadEvent{},
        TouchEvent{},
        StylusEvent{},
        WindowResizeEvent{},
        WindowFocusEvent{},
        WindowCloseRequestEvent{},
    };
    for (auto& ev : evs) {
        std::visit(visitor, ev);
    }
    REQUIRE(visitCount == static_cast<int>(N));
}

// ─── ModFlags bitwise operators ───────────────────────────────────────────────

TEST_CASE("ModFlags OR combines flags correctly", "[unit]")
{
    ModFlags f = ModFlags::Shift | ModFlags::Ctrl;
    REQUIRE(HasFlag(f, ModFlags::Shift));
    REQUIRE(HasFlag(f, ModFlags::Ctrl));
    REQUIRE(!HasFlag(f, ModFlags::Alt));
}

TEST_CASE("ModFlags compound OR assignment works", "[unit]")
{
    ModFlags f = ModFlags::None;
    f |= ModFlags::Alt;
    f |= ModFlags::Super;
    REQUIRE(HasFlag(f, ModFlags::Alt));
    REQUIRE(HasFlag(f, ModFlags::Super));
    REQUIRE(!HasFlag(f, ModFlags::Shift));
}

// ─── InjectInputEvent / DrainInputEvents round-trip ──────────────────────────

TEST_CASE("HeadlessBackend InjectInputEvent and DrainInputEvents round-trip", "[unit]")
{
    Internal::HeadlessBackend backend;
    REQUIRE(backend.Init({}).has_value());

    backend.InjectInputEvent(KeyEvent{ 65, 30, KeyAction::Pressed, ModFlags::None });
    backend.InjectInputEvent(MouseScrollEvent{ 0.0f, -1.0f, false });

    auto events = backend.DrainInputEvents();
    REQUIRE(events.size() == 2);
    REQUIRE(std::holds_alternative<KeyEvent>(events[0]));
    REQUIRE(std::holds_alternative<MouseScrollEvent>(events[1]));

    // Second drain must be empty — events are consumed once.
    auto empty = backend.DrainInputEvents();
    REQUIRE(empty.empty());

    backend.Shutdown();
}

TEST_CASE("HeadlessBackend DrainInputEvents clears queue", "[unit]")
{
    Internal::HeadlessBackend backend;
    REQUIRE(backend.Init({}).has_value());

    backend.InjectInputEvent(TextInputEvent{ U'A' });
    (void)backend.DrainInputEvents();

    auto second = backend.DrainInputEvents();
    REQUIRE(second.empty());

    backend.Shutdown();
}
