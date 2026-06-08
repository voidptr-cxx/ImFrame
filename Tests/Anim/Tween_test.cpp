/**
 * @file     Tween_test.cpp
 * @brief    Unit tests for Anim::Tween
 *
 * @internal
 * All tests are pure arithmetic — no ImGui context or headless backend required.
 * Floating-point comparisons use Catch::Approx with a small epsilon.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Anim/Tween.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ImFrame::Anim::Tween;
using ImFrame::Easing::EaseOutBack;
using ImFrame::Easing::Linear;
using Catch::Approx;

TEST_CASE("Tween reaches target after full duration", "[unit]") {
    Tween<float> t(0.0f, 10.0f, 1.0f, Linear);
    t.Play();
    const float val = t.Update(1.0f);
    REQUIRE(val == Approx(10.0f).epsilon(0.001f));
    REQUIRE(t.IsDone());
}

TEST_CASE("Tween returns from before playback starts", "[unit]") {
    Tween<float> t(3.0f, 10.0f, 1.0f, Linear);
    // Not yet played — Update should return from value
    const float val = t.Update(0.5f);
    REQUIRE(val == Approx(3.0f).epsilon(0.001f));
    REQUIRE_FALSE(t.IsDone());
}

TEST_CASE("EaseOutBack overshoots 1.0 before settling", "[unit]") {
    // With from=0, to=1, EaseOutBack peaks above 1 near the middle
    Tween<float> t(0.0f, 1.0f, 1.0f, EaseOutBack);
    t.Play();
    float maxSeen = 0.0f;
    for (int i = 0; i < 10; ++i) {
        float v = t.Update(0.1f);
        if (v > maxSeen) maxSeen = v;
    }
    REQUIRE(maxSeen > 1.0f);         // confirms overshoot occurred
    REQUIRE(t.Update(0.0f) == Approx(1.0f).epsilon(0.001f));  // settles at target
}

TEST_CASE("OnComplete fires exactly once", "[unit]") {
    int count = 0;
    Tween<float> t(0.0f, 1.0f, 0.5f, Linear);
    t.OnComplete([&count] { ++count; });
    t.Play();

    t.Update(0.3f);
    REQUIRE(count == 0);             // not done yet

    t.Update(0.3f);
    REQUIRE(count == 1);             // just completed

    t.Update(0.3f);
    REQUIRE(count == 1);             // second call must not re-fire
}

TEST_CASE("Tween Pause stops advancement", "[unit]") {
    Tween<float> t(0.0f, 10.0f, 1.0f, Linear);
    t.Play();
    t.Update(0.3f);
    t.Pause();
    const float paused = t.Update(0.0f);

    t.Update(0.5f);                  // should not advance while paused
    REQUIRE(t.Update(0.0f) == Approx(paused).epsilon(0.001f));
    REQUIRE_FALSE(t.IsDone());
}

TEST_CASE("Tween Reverse plays backward from current position", "[unit]") {
    Tween<float> t(0.0f, 10.0f, 1.0f, Linear);
    t.Play();
    t.Update(0.3f);                        // 30% through, value ≈ 3
    const float beforeReverse = t.Update(0.0f);

    t.Reverse();                           // now going from 10 → 0 starting at 70%
    const float atReverse = t.Update(0.0f);
    REQUIRE(atReverse == Approx(beforeReverse).epsilon(0.1f));  // visual continuity

    t.Update(0.1f);
    const float later = t.Update(0.0f);
    REQUIRE(later < beforeReverse);        // moving in the opposite direction
}

TEST_CASE("Tween Restart resets and replays from start", "[unit]") {
    Tween<float> t(0.0f, 10.0f, 1.0f, Linear);
    t.Play();
    t.Update(0.8f);
    REQUIRE_FALSE(t.IsDone());

    t.Restart();
    REQUIRE(t.RawProgress() == Approx(0.0f).epsilon(0.001f));
    REQUIRE(t.IsPlaying());

    const float val = t.Update(0.0f);
    REQUIRE(val == Approx(0.0f).epsilon(0.001f));
}
