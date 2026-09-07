/**
 * @file     Timer_test.cpp
 * @brief    Unit tests for ImFrame::Utility::Timer and TimerHandle
 *
 * @internal
 * Tests verify: After() fires exactly once, Every() fires on each interval,
 * TimerHandle destruction cancels the repeating timer, a callback can schedule
 * new timers without them firing in the same Tick(), and Clear() removes all.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/Timer.hpp"

using namespace ImFrame::Utility;

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("Timer After fires exactly once after elapsed time", "[unit]") {
    Timer timer;
    int callCount = 0;

    timer.After(1.0f, [&callCount]() { ++callCount; });

    timer.Tick(0.5f); // not yet
    REQUIRE(callCount == 0);

    timer.Tick(0.6f); // crosses 1.0 s
    REQUIRE(callCount == 1);

    timer.Tick(1.0f); // past — should NOT fire again
    REQUIRE(callCount == 1);
}

TEST_CASE("Timer After fires on the exact tick it crosses the threshold", "[unit]") {
    Timer timer;
    int callCount = 0;

    timer.After(1.0f, [&callCount]() { ++callCount; });
    timer.Tick(1.0f); // exactly at threshold
    REQUIRE(callCount == 1);
}

TEST_CASE("Timer Every fires on each interval", "[unit]") {
    Timer timer;
    int callCount = 0;

    auto handle = timer.Every(1.0f, [&callCount]() { ++callCount; });

    timer.Tick(1.0f); REQUIRE(callCount == 1);
    timer.Tick(1.0f); REQUIRE(callCount == 2);
    timer.Tick(1.0f); REQUIRE(callCount == 3);
}

TEST_CASE("Timer Every fires multiple times in a single large Tick", "[unit]") {
    Timer timer;
    int callCount = 0;

    auto handle = timer.Every(1.0f, [&callCount]() { ++callCount; });
    timer.Tick(3.5f); // should fire at t=1, t=2, t=3 → 3 times
    REQUIRE(callCount == 3);
}

TEST_CASE("TimerHandle destruction cancels the repeating timer", "[unit]") {
    Timer timer;
    int callCount = 0;

    {
        auto handle = timer.Every(1.0f, [&callCount]() { ++callCount; });
        timer.Tick(1.0f);
        REQUIRE(callCount == 1);
    } // handle destroyed → timer cancelled

    timer.Tick(1.0f);
    REQUIRE(callCount == 1); // no more fires
}

TEST_CASE("TimerHandle::Cancel cancels immediately", "[unit]") {
    Timer timer;
    int callCount = 0;

    auto handle = timer.Every(1.0f, [&callCount]() { ++callCount; });
    handle.Cancel();
    REQUIRE(!handle.IsActive());

    timer.Tick(2.0f);
    REQUIRE(callCount == 0);
}

TEST_CASE("TimerHandle move transfers the timer and empties the source", "[unit]") {
    Timer timer;
    int callCount = 0;

    TimerHandle a = timer.Every(1.0f, [&callCount]() { ++callCount; });
    TimerHandle b = std::move(a);
    REQUIRE(!a.IsActive()); // NOLINT(bugprone-use-after-move)
    REQUIRE(b.IsActive());

    timer.Tick(1.0f);
    REQUIRE(callCount == 1);
}

TEST_CASE("Timer::Clear cancels all active timers", "[unit]") {
    Timer timer;
    int callCount = 0;

    timer.After(0.5f, [&callCount]() { ++callCount; });
    auto h = timer.Every(0.5f, [&callCount]() { ++callCount; });
    timer.Clear();

    timer.Tick(1.0f);
    REQUIRE(callCount == 0);
}

TEST_CASE("Timer callback can schedule new timers without them firing in the same Tick", "[unit]") {
    Timer timer;
    // Pack into a struct so the outer lambda stays within Delegate's 16-byte buffer
    // (at most 2 pointer-sized captures = 16 bytes on 64-bit)
    struct State { int outer{0}; int inner{0}; };
    State state;

    timer.After(1.0f, [&state, &timer]() {
        ++state.outer;
        timer.After(0.0f, [&state]() { ++state.inner; }); // 0 s — fires ASAP
    });

    timer.Tick(1.0f); // outer fires
    REQUIRE(state.outer == 1);
    REQUIRE(state.inner == 0); // inner was scheduled this tick — should NOT fire yet

    timer.Tick(0.0f); // next tick — inner fires
    REQUIRE(state.inner == 1);
}

TEST_CASE("Timer::ActiveCount reflects correct timer count", "[unit]") {
    Timer timer;
    REQUIRE(timer.ActiveCount() == 0);

    timer.After(1.0f, []() {});
    REQUIRE(timer.ActiveCount() == 1);

    auto h = timer.Every(0.5f, []() {});
    REQUIRE(timer.ActiveCount() == 2);

    timer.Tick(1.5f); // After fires and is removed
    REQUIRE(timer.ActiveCount() == 1); // Only Every remains

    h.Cancel();
    REQUIRE(timer.ActiveCount() == 0);
}

TEST_CASE("Timer multiple one-shots all fire in the same Tick", "[unit]") {
    Timer timer;
    int callCount = 0;

    for (int i = 0; i < 5; ++i)
        timer.After(0.5f, [&callCount]() { ++callCount; });

    timer.Tick(1.0f);
    REQUIRE(callCount == 5);
}
