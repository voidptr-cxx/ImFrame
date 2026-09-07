/**
 * @file     State_test.cpp
 * @brief    Unit tests for State<T>, Tree::Signal<T>, and Computed<T>
 *
 * These tests exercise the Phase 28 reactive state primitives without an
 * ImGui context. The dirty-registration mechanism (`StateRegistrarScope` /
 * `g_stateRegistrar`) is driven directly via the RAII scopes in
 * `ImFrame::Internal::`, allowing full coverage of the dirty-notification
 * path without requiring a live `ComponentElement`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-02
 * @version  2.3.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Tree/Computed.hpp"
#include "ImFrame/Tree/Signal.hpp"
#include "ImFrame/Tree/State.hpp"
#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Tree/Primitives/SizedBox.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Tree;
using namespace ImFrame::Tree::Primitives;

// ─── State<T> tests ───────────────────────────────────────────────────────────

TEST_CASE("State<T>: Get returns default-constructed T outside Build context", "[tree][state]") {
    State<int> s;
    CHECK(s.Get() == 0);
}

TEST_CASE("State<T>: Set applies mutation immediately", "[tree][state]") {
    State<int> s;
    s.Set([](int& v) { v = 42; });
    CHECK(s.Get() == 42);
}

TEST_CASE("State<T>: Get during Build registers dirty callback", "[tree][state]") {
    State<int> s;
    bool fired = false;
    std::function<void()> markDirty = [&fired] { fired = true; };
    {
        Internal::StateRegistrarScope scope{markDirty};
        [[maybe_unused]] const int& _ = s.Get();
    }
    REQUIRE_FALSE(fired);
    s.Set([](int& v) { v = 1; });
    CHECK(fired);
}

TEST_CASE("State<T>: Set outside Build context does not crash when no callback", "[tree][state]") {
    State<int> s;
    REQUIRE_NOTHROW(s.Set([](int& v) { v = 99; }));
    CHECK(s.Get() == 99);
}

TEST_CASE("State<T>: copies share the same Node - mutation visible via all handles", "[tree][state]") {
    State<int> s1;
    State<int> s2 = s1;  // copy — same Node
    s1.Set([](int& v) { v = 7; });
    CHECK(s2.Get() == 7);
}

TEST_CASE("State<T>: multiple Set() calls - dirty callback fires each time", "[tree][state]") {
    State<int> s;
    int callCount = 0;
    std::function<void()> markDirty = [&callCount] { ++callCount; };
    {
        Internal::StateRegistrarScope scope{markDirty};
        [[maybe_unused]] const int& _ = s.Get();
    }
    s.Set([](int& v) { v = 1; });
    s.Set([](int& v) { v = 2; });
    s.Set([](int& v) { v = 3; });
    CHECK(callCount == 3);
    // The dirty flag on a ComponentElement is atomic — multiple fires are idempotent
    // (the flag is already true after the first; the element rebuilds once per frame)
}

TEST_CASE("State<T>: Get refreshes dirty callback on every Build pass", "[tree][state]") {
    State<int> s;
    int fires1 = 0;
    int fires2 = 0;
    std::function<void()> markDirty1 = [&fires1] { ++fires1; };
    std::function<void()> markDirty2 = [&fires2] { ++fires2; };
    {
        Internal::StateRegistrarScope scope{markDirty1};
        [[maybe_unused]] const int& _ = s.Get();
    }
    // Second Build pass updates to markDirty2
    {
        Internal::StateRegistrarScope scope{markDirty2};
        [[maybe_unused]] const int& _ = s.Get();
    }
    s.Set([](int& v) { v = 1; });
    CHECK(fires1 == 0);  // old callback replaced
    CHECK(fires2 == 1);
}

// ─── ComponentElement dirty-skip integration ──────────────────────────────────

TEST_CASE("ComponentElement: Build skipped when not dirty", "[tree][state][component]") {
    struct Counter {
        State<int>             state;
        std::shared_ptr<int>   buildCount = std::make_shared<int>(0);

        Widget Build() const {
            ++*buildCount;
            [[maybe_unused]] const int& _ = state.Get();
            return SizedBox{};
        }
    };

    Counter c;
    Widget  w(c);
    auto    elem = w.CreateElement();
    elem->Mount(nullptr, 0, w);
    REQUIRE(*c.buildCount == 1);

    // No dirty: build should NOT run again
    elem->Update(w);
    CHECK(*c.buildCount == 1);

    // Set state → dirty
    c.state.Set([](int& v) { v = 42; });
    elem->Update(w);
    CHECK(*c.buildCount == 2);
}

TEST_CASE("ComponentElement: GetDirtyFlag returns a valid weak_ptr", "[tree][state][component]") {
    struct Leaf {
        Widget Build() const { return SizedBox{}; }
    };
    Widget  w{Leaf{}};
    auto    elem = w.CreateElement();
    elem->Mount(nullptr, 0, w);

    auto weakFlag = elem->GetDirtyFlag();
    // After mounting, the flag is false (Rebuild cleared it)
    auto flag = weakFlag.lock();
    REQUIRE(flag);
    CHECK_FALSE(flag->load());
}

// ─── Tree::Signal<T> tests ────────────────────────────────────────────────────

TEST_CASE("Signal<T>: default-constructed value", "[tree][signal]") {
    Signal<int> s;
    CHECK(s.Get() == 0);
}

TEST_CASE("Signal<T>: assignment updates stored value", "[tree][signal]") {
    Signal<int> s(0);
    s = 42;
    CHECK(s.Get() == 42);
}

TEST_CASE("Signal<T>: Get during Build registers dirty callback", "[tree][signal]") {
    Signal<std::string> s{"hello"};
    bool fired = false;
    std::function<void()> markDirty = [&fired] { fired = true; };
    {
        Internal::StateRegistrarScope scope{markDirty};
        [[maybe_unused]] auto _ = s.Get();
    }
    REQUIRE_FALSE(fired);
    s = "world";
    CHECK(fired);
}

TEST_CASE("Signal<T>: copies share the same Node", "[tree][signal]") {
    Signal<int> s1(0);
    Signal<int> s2 = s1;
    s1 = 99;
    CHECK(s2.Get() == 99);
}

TEST_CASE("Signal<T>: OnChange subscription fires on assignment", "[tree][signal]") {
    Signal<int> s(0);
    int value = -1;
    auto conn = s.OnChange([&s, &value] { value = s.Get(); });
    s = 55;
    CHECK(value == 55);
}

TEST_CASE("Signal<T>: OnChange connection disconnect stops firing", "[tree][signal]") {
    Signal<int> s(0);
    int callCount = 0;
    {
        auto conn = s.OnChange([&callCount] { ++callCount; });
        s = 1;
        CHECK(callCount == 1);
    } // conn destroyed → disconnected
    s = 2;
    CHECK(callCount == 1);  // did not fire again
}

TEST_CASE("Signal<T>: assignment from background thread fires dirty callback", "[tree][signal][tsan]") {
    Signal<int>  s(0);
    std::atomic<int> received{-1};
    std::function<void()> markDirty = [&s, &received] {
        received.store(s.Get(), std::memory_order_release);
    };
    {
        Internal::StateRegistrarScope scope{markDirty};
        [[maybe_unused]] auto _ = s.Get();
    }
    std::thread t([&s] { s = 77; });
    t.join();
    CHECK(received.load(std::memory_order_acquire) == 77);
}

// ─── Computed<T> tests ────────────────────────────────────────────────────────

TEST_CASE("Computed<T>: initial value computed eagerly at construction", "[tree][computed]") {
    Signal<int>  a(3);
    Signal<int>  b(4);
    Computed<int> sum{[a, b] { return a.Get() + b.Get(); }, a, b};
    CHECK(sum.Value() == 7);
}

TEST_CASE("Computed<T>: recomputes when a dep signal changes", "[tree][computed]") {
    Signal<int>  a(10);
    Signal<int>  b(5);
    Computed<int> diff{[a, b] { return a.Get() - b.Get(); }, a, b};
    REQUIRE(diff.Value() == 5);
    b = 3;
    CHECK(diff.Value() == 7);
}

TEST_CASE("Computed<T>: value is cached - fn not called twice on consecutive reads", "[tree][computed]") {
    Signal<int>   a(1);
    int           callCount = 0;
    Computed<int> c{[a, &callCount] { ++callCount; return a.Get() * 2; }, a};
    REQUIRE(c.Value() == 2);  // initial computation already happened in ctor
    REQUIRE(callCount == 1);
    [[maybe_unused]] const int& v1 = c.Value();  // cache hit
    CHECK(callCount == 1);
    a = 5;
    [[maybe_unused]] const int& v2 = c.Value();  // recompute
    CHECK(callCount == 2);
    [[maybe_unused]] const int& v3 = c.Value();  // cache hit
    CHECK(callCount == 2);
}

TEST_CASE("Computed<T>: no-dep computed never changes after first eval", "[tree][computed]") {
    int          callCount = 0;
    Computed<int> c{[&callCount] { ++callCount; return 42; }};
    REQUIRE(c.Value() == 42);
    REQUIRE(callCount == 1);
    [[maybe_unused]] const int& v = c.Value();
    CHECK(callCount == 1);
}

TEST_CASE("Computed<T>: dirty notification cascades to building element via onDirty", "[tree][computed]") {
    Signal<int>   a(1);
    Computed<int> doubled{[a] { return a.Get() * 2; }, a};

    bool elemDirty = false;
    std::function<void()> markDirty = [&elemDirty] { elemDirty = true; };
    {
        Internal::StateRegistrarScope scope{markDirty};
        [[maybe_unused]] const int& _ = doubled.Value();  // registers markDirty as onDirty
    }
    REQUIRE_FALSE(elemDirty);
    a = 10;  // dep changes → computed dirty → fires onDirty → elemDirty set
    CHECK(elemDirty);
    CHECK(doubled.Value() == 20);  // recomputed
}
