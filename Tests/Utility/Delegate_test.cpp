/**
 * @file     Delegate_test.cpp
 * @brief    Unit tests for ImFrame::Utility::Delegate<R(Args...)>
 *
 * @internal
 * Tests verify: free function invocation, member function invocation via Bind,
 * lambda/functor invocation, operator bool for empty and assigned delegates,
 * Reset() clearing the target, copy and move semantics, and that no heap
 * allocation occurs (verified by confirming all supported callables fit within
 * Delegate::BUFFER_SIZE at compile time).
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

#include "ImFrame/Utility/Delegate.hpp"

#include <string>

using namespace ImFrame::Utility;

// ─── Helper free functions ────────────────────────────────────────────────────

static int DoubleInt(int x) { return x * 2; }
static void IncrementRef(int& x) { ++x; }
static std::string Concat(std::string a, std::string b) { return a + b; }

// ─── Helper class for member-function tests ───────────────────────────────────

struct Calculator {
    int bias{0};
    int Add(int x) const { return x + bias; }
    void Accumulate(int x) { bias += x; }
};

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("Delegate default-constructed is empty", "[unit]") {
    Delegate<int(int)> d;
    REQUIRE(!d);
}

TEST_CASE("Delegate operator bool is true after assigning a target", "[unit]") {
    Delegate<int(int)> d{&DoubleInt};
    REQUIRE(static_cast<bool>(d));
}

TEST_CASE("Delegate invokes a free function correctly", "[unit]") {
    Delegate<int(int)> d{&DoubleInt};
    REQUIRE(d(21) == 42);
}

TEST_CASE("Delegate invokes a void free function with reference argument", "[unit]") {
    Delegate<void(int&)> d{&IncrementRef};
    int value = 0;
    d(value);
    REQUIRE(value == 1);
}

TEST_CASE("Delegate invokes a free function with multiple arguments", "[unit]") {
    Delegate<std::string(std::string, std::string)> d{&Concat};
    REQUIRE(d("hello", " world") == "hello world");
}

TEST_CASE("Delegate::Bind invokes a const member function", "[unit]") {
    Calculator calc;
    calc.bias = 10;
    auto d = Delegate<int(int)>::Bind<&Calculator::Add>(&calc);
    REQUIRE(d(5) == 15);
}

TEST_CASE("Delegate::Bind invokes a mutating member function", "[unit]") {
    Calculator calc;
    auto d = Delegate<void(int)>::Bind<&Calculator::Accumulate>(&calc);
    d(7);
    d(3);
    REQUIRE(calc.bias == 10);
}

TEST_CASE("Delegate invokes a captureless lambda", "[unit]") {
    Delegate<int(int)> d{[](int x) { return x + 1; }};
    REQUIRE(d(99) == 100);
}

TEST_CASE("Delegate invokes a lambda capturing a pointer", "[unit]") {
    int multiplier = 3;
    Delegate<int(int)> d{[&multiplier](int x) { return x * multiplier; }};
    REQUIRE(d(7) == 21);
}

TEST_CASE("Delegate void() fire-and-forget invocation", "[unit]") {
    int counter = 0;
    Delegate<void()> d{[&counter]() { ++counter; }};
    d();
    d();
    REQUIRE(counter == 2);
}

TEST_CASE("Delegate::Reset clears the target", "[unit]") {
    Delegate<int(int)> d{&DoubleInt};
    REQUIRE(static_cast<bool>(d));
    d.Reset();
    REQUIRE(!d);
}

TEST_CASE("Delegate can be reassigned after Reset", "[unit]") {
    Delegate<int(int)> d{&DoubleInt};
    d.Reset();
    d = Delegate<int(int)>{[](int x) { return x + 100; }};
    REQUIRE(d(1) == 101);
}

TEST_CASE("Delegate copy constructor preserves the target", "[unit]") {
    Delegate<int(int)> original{&DoubleInt};
    Delegate<int(int)> copy{original};
    REQUIRE(copy(5) == 10);
    REQUIRE(original(5) == 10); // original unaffected
}

TEST_CASE("Delegate copy assignment preserves the target", "[unit]") {
    Delegate<int(int)> a{&DoubleInt};
    Delegate<int(int)> b;
    b = a;
    REQUIRE(b(3) == 6);
}

TEST_CASE("Delegate move constructor transfers target and empties source", "[unit]") {
    Delegate<int(int)> source{&DoubleInt};
    Delegate<int(int)> dest{std::move(source)};
    REQUIRE(dest(4) == 8);
    REQUIRE(!source); // NOLINT(bugprone-use-after-move) — intentional post-move check
}

TEST_CASE("Delegate move assignment transfers target and empties source", "[unit]") {
    Delegate<int(int)> source{[](int x) { return x * 3; }};
    Delegate<int(int)> dest;
    dest = std::move(source);
    REQUIRE(dest(4) == 12);
    REQUIRE(!source); // NOLINT(bugprone-use-after-move)
}

TEST_CASE("Delegate BUFFER_SIZE fits free functions and object pointers", "[unit]") {
    // Compile-time check: all supported target types must fit
    static_assert(sizeof(void(*)()) <= Delegate<void()>::BUFFER_SIZE,
                  "free function pointer must fit in Delegate buffer");
    static_assert(sizeof(void*) <= Delegate<void()>::BUFFER_SIZE,
                  "object pointer must fit in Delegate buffer");
    SUCCEED("buffer size static assertions passed");
}

TEST_CASE("Delegate bool(float) typed callback invocation", "[unit]") {
    Delegate<bool(float)> d{[](float v) { return v > 0.5f; }};
    REQUIRE(d(0.9f) == true);
    REQUIRE(d(0.1f) == false);
}
