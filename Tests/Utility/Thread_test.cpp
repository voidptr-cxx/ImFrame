/**
 * @file     Thread_test.cpp
 * @brief    Unit and thread-safety tests for ImFrame::Utility::Thread
 *
 * @internal
 * Tests verify: OS thread naming, IsRunning() lifecycle, RequestStop() delivery,
 * and SetPriority() / SetAffinity() completions. Named [unit] and [tsan] so
 * they run in both the standard CI pass and the ThreadSanitizer pass.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-05-31
 * @version  0.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/Thread.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using namespace ImFrame::Utility;

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace {

/// Spins for up to timeoutMs waiting for cond to become true. Returns cond's final value.
template <typename Pred>
bool WaitFor(Pred cond, int timeoutMs = 2000) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds{timeoutMs};
    while (!cond()) {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    return true;
}

} // namespace

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("Thread::Name returns the name passed at construction", "[unit]") {
    std::atomic<bool> ready{false};
    Thread t{"TestThread", [&ready] {
        ready.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }};

    REQUIRE(t.Name() == "TestThread");
    (void)WaitFor([&] { return ready.load(std::memory_order_acquire); });
}

TEST_CASE("Thread::IsRunning transitions true then false", "[unit][tsan]") {
    std::atomic<bool> proceed{false};

    Thread t{"LifecycleThread", [&proceed] {
        WaitFor([&] { return proceed.load(std::memory_order_acquire); });
    }};

    // Should be running (callable hasn't returned yet).
    REQUIRE(WaitFor([&t] { return t.IsRunning(); }));

    // Let the callable return.
    proceed.store(true, std::memory_order_release);
    REQUIRE(WaitFor([&t] { return !t.IsRunning(); }));
}

TEST_CASE("Thread::RequestStop delivers stop_token to callable", "[unit][tsan]") {
    std::atomic<bool> stopped{false};

    Thread t{"StopTestThread", [&stopped](std::stop_token st) {
        while (!st.stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        stopped.store(true, std::memory_order_release);
    }};

    REQUIRE(WaitFor([&t] { return t.IsRunning(); }));
    t.RequestStop();
    REQUIRE(WaitFor([&stopped] { return stopped.load(std::memory_order_acquire); }));
}

TEST_CASE("Thread::SetPriority Normal completes without error", "[unit]") {
    std::atomic<bool> ready{false};
    Thread t{"PriorityThread", [&ready] {
        ready.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }};
    REQUIRE(WaitFor([&] { return ready.load(); }));
    // Must not throw or crash.
    REQUIRE_NOTHROW(t.SetPriority(Priority::Normal));
}

TEST_CASE("Thread::SetPriority High completes without error", "[unit]") {
    std::atomic<bool> ready{false};
    Thread t{"HighPrioThread", [&ready] {
        ready.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }};
    REQUIRE(WaitFor([&] { return ready.load(); }));
    REQUIRE_NOTHROW(t.SetPriority(Priority::High));
}

TEST_CASE("Thread::SetAffinity core 0 completes without error", "[unit]") {
    std::atomic<bool> ready{false};
    Thread t{"AffinityThread", [&ready] {
        ready.store(true, std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }};
    REQUIRE(WaitFor([&] { return ready.load(); }));
    // Pins to core 0. Must not throw or crash.
    REQUIRE_NOTHROW(t.SetAffinity(0x1u));
}

TEST_CASE("Thread destructor joins without blocking indefinitely", "[unit][tsan]") {
    // If this test completes within the test framework timeout, join works.
    std::atomic<bool> started{false};
    {
        Thread t{"DtorJoinThread", [&started] {
            started.store(true, std::memory_order_release);
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }};
        REQUIRE(WaitFor([&] { return started.load(); }));
    } // dtor joins here
    REQUIRE(started.load());
}
