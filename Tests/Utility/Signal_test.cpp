/**
 * @file     Signal_test.cpp
 * @brief    Unit and thread-safety tests for Signal and ThreadSafeSignal
 *
 * @internal
 * Tests verify: connect + emit round-trip, connection-destruction disconnects,
 * emit order matches connection order, re-entrant disconnect during emit is safe,
 * DisconnectAll removes all handlers, and ThreadSafeSignal passes TSan under
 * concurrent emit and connect from multiple threads.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/Signal.hpp"

#include <atomic>
#include <thread>
#include <vector>

using namespace ImFrame::Utility;

// ─── Signal<void(Args...)> tests ──────────────────────────────────────────────

TEST_CASE("Signal connect and emit round-trip", "[unit]") {
    Signal<void(int)> sig;
    int received = -1;

    auto conn = sig.Connect([&received](int v) { received = v; });
    sig.Emit(42);

    REQUIRE(received == 42);
}

TEST_CASE("Signal connection destruction disconnects the handler", "[unit]") {
    Signal<void()> sig;
    int callCount = 0;

    {
        auto conn = sig.Connect([&callCount]() { ++callCount; });
        sig.Emit();
        REQUIRE(callCount == 1);
    } // conn destroyed → handler removed

    sig.Emit();
    REQUIRE(callCount == 1); // still 1 — handler was removed
}

TEST_CASE("Signal Connection::Disconnect removes handler immediately", "[unit]") {
    Signal<void()> sig;
    int callCount = 0;

    auto conn = sig.Connect([&callCount]() { ++callCount; });
    sig.Emit();
    conn.Disconnect();
    sig.Emit();

    REQUIRE(callCount == 1);
    REQUIRE(!conn.IsConnected());
}

TEST_CASE("Signal emit order matches connection order", "[unit]") {
    Signal<void()> sig;
    std::vector<int> order;

    auto c1 = sig.Connect([&order]() { order.push_back(1); });
    auto c2 = sig.Connect([&order]() { order.push_back(2); });
    auto c3 = sig.Connect([&order]() { order.push_back(3); });
    sig.Emit();

    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("Signal ConnectionCount returns correct count", "[unit]") {
    Signal<void()> sig;
    REQUIRE(sig.ConnectionCount() == 0);

    auto c1 = sig.Connect([]() {});
    REQUIRE(sig.ConnectionCount() == 1);

    auto c2 = sig.Connect([]() {});
    REQUIRE(sig.ConnectionCount() == 2);

    c1.Disconnect();
    REQUIRE(sig.ConnectionCount() == 1);
}

TEST_CASE("Signal DisconnectAll removes all handlers", "[unit]") {
    Signal<void()> sig;
    int callCount = 0;

    auto c1 = sig.Connect([&callCount]() { ++callCount; });
    auto c2 = sig.Connect([&callCount]() { ++callCount; });
    sig.DisconnectAll();
    sig.Emit();

    REQUIRE(callCount == 0);
    // Tokens from disconnected slots — IsConnected reflects stale state but
    // subsequent Disconnect() is a no-op (weak_ptr expired after DisconnectAll clears)
}

TEST_CASE("Signal re-entrant disconnect during emit is safe", "[unit]") {
    Signal<void()> sig;
    int callCount = 0;

    Connection selfDisconn;
    selfDisconn = sig.Connect([&callCount, &selfDisconn]() {
        ++callCount;
        selfDisconn.Disconnect(); // disconnect self during emit
    });

    sig.Emit(); // handler runs once
    sig.Emit(); // handler should NOT run — it disconnected itself

    REQUIRE(callCount == 1);
}

TEST_CASE("Signal Connection move preserves active subscription", "[unit]") {
    Signal<void()> sig;
    int callCount = 0;

    Connection a = sig.Connect([&callCount]() { ++callCount; });
    Connection b = std::move(a);
    REQUIRE(!a.IsConnected()); // NOLINT(bugprone-use-after-move) — intentional
    REQUIRE(b.IsConnected());

    sig.Emit();
    REQUIRE(callCount == 1);
}

TEST_CASE("Signal: connection after signal destroyed is safe no-op", "[unit]") {
    Connection savedConn;
    {
        Signal<void()> sig;
        savedConn = sig.Connect([]() {});
    } // sig destroyed

    // Destructor of savedConn: weak_ptr expired → no-op
    REQUIRE_NOTHROW(savedConn.Disconnect());
}

// ─── ThreadSafeSignal tests ───────────────────────────────────────────────────

TEST_CASE("ThreadSafeSignal connect and emit round-trip", "[unit]") {
    ThreadSafeSignal<void(int)> sig;
    std::atomic<int> received{-1};

    auto conn = sig.Connect([&received](int v) {
        received.store(v, std::memory_order_relaxed);
    });
    sig.Emit(99);

    REQUIRE(received.load(std::memory_order_acquire) == 99);
}

TEST_CASE("ThreadSafeSignal connection destruction disconnects handler", "[unit]") {
    ThreadSafeSignal<void()> sig;
    std::atomic<int> callCount{0};

    {
        auto conn = sig.Connect([&callCount]() {
            callCount.fetch_add(1, std::memory_order_relaxed);
        });
        sig.Emit();
    }
    sig.Emit();

    REQUIRE(callCount.load(std::memory_order_acquire) == 1);
}

TEST_CASE("ThreadSafeSignal concurrent emit and connect do not data-race", "[unit][tsan]") {
    ThreadSafeSignal<void()> sig;
    std::atomic<int> emitCount{0};

    static constexpr int EMITTERS    = 4;
    static constexpr int EMITS_EACH  = 200;
    static constexpr int CONNECTORS  = 2;
    static constexpr int CONNECTS_EACH = 50;

    std::vector<Connection> connections;
    connections.reserve(CONNECTORS * CONNECTS_EACH);
    std::mutex connMtx;

    std::vector<std::jthread> threads;
    threads.reserve(EMITTERS + CONNECTORS);

    for (int i = 0; i < EMITTERS; ++i) {
        threads.emplace_back([&sig, &emitCount]() {
            for (int j = 0; j < EMITS_EACH; ++j)
                sig.Emit();
        });
    }

    for (int i = 0; i < CONNECTORS; ++i) {
        threads.emplace_back([&sig, &emitCount, &connections, &connMtx]() {
            for (int j = 0; j < CONNECTS_EACH; ++j) {
                auto conn = sig.Connect([&emitCount]() {
                    emitCount.fetch_add(1, std::memory_order_relaxed);
                });
                std::lock_guard lock{connMtx};
                connections.push_back(std::move(conn));
            }
        });
    }

    threads.clear(); // join all

    // Final emit with all connections active — should not data-race
    sig.Emit();
    REQUIRE(sig.ConnectionCount() == static_cast<std::size_t>(CONNECTORS * CONNECTS_EACH));
}
