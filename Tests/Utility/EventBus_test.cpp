/**
 * @file     EventBus_test.cpp
 * @brief    Unit and thread-safety tests for ImFrame::Utility::EventBus
 *
 * @internal
 * Tests verify: subscribe + dispatch round-trip, token destruction unsubscribes,
 * dispatch during handler does not corrupt the iteration (re-entrant safety),
 * DispatchAsync handler runs on a thread other than the caller, concurrent
 * Dispatch from 4 threads passes TSan, and SubscriptionToken move preserves
 * the subscription.
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

#include "ImFrame/Utility/EventBus.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace ImFrame::Utility;
using namespace std::chrono_literals;

// ─── Event types used in tests ────────────────────────────────────────────────

struct IntEvent    { int value; };
struct StringEvent { std::string text; };
struct TagEvent    {}; // zero-payload event

// ─── Helper: busy-wait up to timeout_ms for a condition ──────────────────────

static bool WaitFor(std::atomic<bool>& flag, int timeoutMs = 2000) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!flag.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::yield();
    }
    return true;
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("EventBus subscribe and synchronous dispatch round-trip", "[unit]") {
    EventBus bus;
    int received = -1;

    auto token = bus.Subscribe<IntEvent>([&received](const IntEvent& e) {
        received = e.value;
    });

    bus.Dispatch(IntEvent{42});
    REQUIRE(received == 42);
}

TEST_CASE("EventBus dispatches multiple handlers in registration order", "[unit]") {
    EventBus bus;
    std::vector<int> order;

    auto t1 = bus.Subscribe<TagEvent>([&order](const TagEvent&) { order.push_back(1); });
    auto t2 = bus.Subscribe<TagEvent>([&order](const TagEvent&) { order.push_back(2); });
    auto t3 = bus.Subscribe<TagEvent>([&order](const TagEvent&) { order.push_back(3); });
    bus.Dispatch(TagEvent{});

    REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("EventBus token destruction unsubscribes the handler", "[unit]") {
    EventBus bus;
    int callCount = 0;

    {
        auto token = bus.Subscribe<IntEvent>([&callCount](const IntEvent&) { ++callCount; });
        bus.Dispatch(IntEvent{1});
        REQUIRE(callCount == 1);
    } // token destroyed → handler removed

    bus.Dispatch(IntEvent{2});
    REQUIRE(callCount == 1);
}

TEST_CASE("EventBus SubscriptionToken::Unsubscribe removes handler immediately", "[unit]") {
    EventBus bus;
    int callCount = 0;

    auto token = bus.Subscribe<IntEvent>([&callCount](const IntEvent&) { ++callCount; });
    bus.Dispatch(IntEvent{1});
    token.Unsubscribe();
    bus.Dispatch(IntEvent{2});

    REQUIRE(callCount == 1);
    REQUIRE(!token.IsSubscribed());
}

TEST_CASE("EventBus dispatch with no subscribers is a no-op", "[unit]") {
    EventBus bus;
    REQUIRE_NOTHROW(bus.Dispatch(IntEvent{0}));
}

TEST_CASE("EventBus independent event types do not interfere", "[unit]") {
    EventBus bus;
    int intCount    = 0;
    int stringCount = 0;

    auto t1 = bus.Subscribe<IntEvent>   ([&intCount]   (const IntEvent&)    { ++intCount; });
    auto t2 = bus.Subscribe<StringEvent>([&stringCount](const StringEvent&)  { ++stringCount; });

    bus.Dispatch(IntEvent{1});
    bus.Dispatch(IntEvent{2});
    bus.Dispatch(StringEvent{"hi"});

    REQUIRE(intCount    == 2);
    REQUIRE(stringCount == 1);
}

TEST_CASE("EventBus dispatch during handler does not corrupt iteration", "[unit]") {
    EventBus bus;
    int outerCount = 0;
    int innerCount = 0;

    auto t1 = bus.Subscribe<IntEvent>([&bus, &outerCount, &innerCount](const IntEvent& e) {
        ++outerCount;
        if (e.value == 1) {
            // Dispatch a second event from inside a handler
            bus.Dispatch(IntEvent{2});
        } else {
            ++innerCount;
        }
    });

    bus.Dispatch(IntEvent{1});

    REQUIRE(outerCount == 2); // handler called for value=1 and value=2
    REQUIRE(innerCount == 1); // inner dispatch saw value=2
}

TEST_CASE("EventBus SubscriptionToken move preserves subscription", "[unit]") {
    EventBus bus;
    int callCount = 0;

    SubscriptionToken a = bus.Subscribe<TagEvent>([&callCount](const TagEvent&) { ++callCount; });
    SubscriptionToken b = std::move(a);

    REQUIRE(!a.IsSubscribed()); // NOLINT(bugprone-use-after-move) — intentional
    REQUIRE(b.IsSubscribed());

    bus.Dispatch(TagEvent{});
    REQUIRE(callCount == 1);
}

TEST_CASE("EventBus token destruction after bus destruction is safe", "[unit]") {
    SubscriptionToken token;
    {
        EventBus bus;
        token = bus.Subscribe<TagEvent>([](const TagEvent&) {});
    } // bus destroyed; token holds a weak_ptr that is now expired
    REQUIRE_NOTHROW(token.Unsubscribe()); // must be a no-op
}

TEST_CASE("EventBus DispatchAsync handler runs on a different thread", "[unit][tsan]") {
    EventBus bus;
    std::atomic<bool> handled{false};
    const auto callerThreadId = std::this_thread::get_id();
    std::atomic<bool> ranOnDifferentThread{false};

    auto token = bus.Subscribe<TagEvent>([&](const TagEvent&) {
        ranOnDifferentThread.store(
            std::this_thread::get_id() != callerThreadId,
            std::memory_order_release);
        handled.store(true, std::memory_order_release);
    });

    bus.DispatchAsync(TagEvent{});
    REQUIRE(WaitFor(handled));
    REQUIRE(ranOnDifferentThread.load(std::memory_order_acquire));
}

TEST_CASE("EventBus DispatchAsync delivers event value correctly", "[unit][tsan]") {
    EventBus bus;
    std::atomic<int> received{-1};
    std::atomic<bool> handled{false};

    auto token = bus.Subscribe<IntEvent>([&](const IntEvent& e) {
        received.store(e.value, std::memory_order_release);
        handled.store(true, std::memory_order_release);
    });

    bus.DispatchAsync(IntEvent{99});
    REQUIRE(WaitFor(handled));
    REQUIRE(received.load(std::memory_order_acquire) == 99);
}

TEST_CASE("EventBus concurrent Dispatch from 4 threads does not data-race", "[tsan]") {
    EventBus bus;
    std::atomic<int> counter{0};

    static constexpr int THREADS     = 4;
    static constexpr int DISPATCHES  = 250;

    auto token = bus.Subscribe<IntEvent>([&counter](const IntEvent&) {
        counter.fetch_add(1, std::memory_order_relaxed);
    });

    std::vector<std::jthread> threads;
    threads.reserve(THREADS);

    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&bus]() {
            for (int i = 0; i < DISPATCHES; ++i)
                bus.Dispatch(IntEvent{i});
        });
    }

    threads.clear(); // join all
    REQUIRE(counter.load(std::memory_order_acquire) == THREADS * DISPATCHES);
}

TEST_CASE("EventBus concurrent Subscribe and Dispatch do not data-race", "[tsan]") {
    EventBus bus;
    std::atomic<int> dispatchCount{0};

    static constexpr int SUBSCRIBERS = 4;
    static constexpr int DISPATCHERS = 4;
    static constexpr int OPS_EACH    = 100;

    std::vector<std::jthread> threads;
    threads.reserve(SUBSCRIBERS + DISPATCHERS);

    std::vector<SubscriptionToken> tokens;
    std::mutex tokenMtx;

    for (int i = 0; i < SUBSCRIBERS; ++i) {
        threads.emplace_back([&bus, &tokens, &tokenMtx, &dispatchCount]() {
            for (int j = 0; j < OPS_EACH; ++j) {
                auto tok = bus.Subscribe<IntEvent>([&dispatchCount](const IntEvent&) {
                    dispatchCount.fetch_add(1, std::memory_order_relaxed);
                });
                std::lock_guard lock{tokenMtx};
                tokens.push_back(std::move(tok));
            }
        });
    }

    for (int i = 0; i < DISPATCHERS; ++i) {
        threads.emplace_back([&bus]() {
            for (int j = 0; j < OPS_EACH; ++j)
                bus.Dispatch(IntEvent{j});
        });
    }

    threads.clear(); // join all — no REQUIRE on exact count since subscribe/dispatch race is intentional
    SUCCEED("no data race detected by TSan");
}
