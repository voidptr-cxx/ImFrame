/**
 * @file     AnimatedValue_test.cpp
 * @brief    Unit tests for Anim::AnimatedValue
 *
 * @internal
 * All tests are pure arithmetic — no ImGui context or headless backend required.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Anim/AnimatedValue.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ImFrame::Anim::AnimatedValue;
using Catch::Approx;

TEST_CASE("AnimatedValue approaches target within tolerance after sufficient updates", "[unit]") {
    AnimatedValue<float> v(0.0f, 8.0f);
    v.SetTarget(1.0f);

    // After 1 second at speed 8 the value should be within 0.01 of target
    // (1 - exp(-8) ≈ 0.9997)
    for (int i = 0; i < 60; ++i) v.Update(1.0f / 60.0f);

    REQUIRE(v.Value() == Approx(1.0f).epsilon(0.01f));
}

TEST_CASE("Higher speed converges faster", "[unit]") {
    AnimatedValue<float> slow(0.0f, 1.0f);   // default speed 8
    AnimatedValue<float> fast(0.0f, 20.0f);  // faster
    slow.SetTarget(1.0f);
    fast.SetTarget(1.0f);

    constexpr float dt = 0.1f;
    slow.Update(dt);
    fast.Update(dt);

    // fast should be closer to target after the same dt
    const float distSlow = 1.0f - slow.Value();
    const float distFast = 1.0f - fast.Value();
    REQUIRE(distFast < distSlow);
}

TEST_CASE("AnimatedValue SnapToTarget is immediate", "[unit]") {
    AnimatedValue<float> v(0.0f, 8.0f);
    v.SetTarget(5.0f);
    v.SnapToTarget();
    REQUIRE(v.Value() == Approx(5.0f).epsilon(0.0001f));
}

TEST_CASE("AnimatedValue frame-rate independence", "[unit]") {
    // The formula current + (target - current) * (1 - exp(-speed*dt)) is
    // exactly frame-rate independent: n steps of dt/n equals one step of dt.
    AnimatedValue<float> fine(0.0f, 8.0f);
    AnimatedValue<float> coarse(0.0f, 8.0f);
    fine.SetTarget(1.0f);
    coarse.SetTarget(1.0f);

    for (int i = 0; i < 60; ++i) fine.Update(1.0f / 60.0f);  // 60 × 1/60 s
    coarse.Update(1.0f);                                        // 1 × 1 s

    REQUIRE(fine.Value() == Approx(coarse.Value()).epsilon(0.001f));
}

TEST_CASE("AnimatedValue tracks changing target", "[unit]") {
    AnimatedValue<float> v(0.0f, 8.0f);
    v.SetTarget(1.0f);
    for (int i = 0; i < 30; ++i) v.Update(1.0f / 60.0f);

    // Change target mid-flight
    v.SetTarget(-1.0f);
    for (int i = 0; i < 120; ++i) v.Update(1.0f / 60.0f);

    REQUIRE(v.Value() == Approx(-1.0f).epsilon(0.01f));
}
