/**
 * @file     Sequence_test.cpp
 * @brief    Unit tests for Anim::AnimSequence
 *
 * @internal
 * All tests are pure arithmetic — no ImGui context or headless backend required.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Anim/Sequence.hpp"
#include "ImFrame/Anim/Tween.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ImFrame::Anim::AnimSequence;
using ImFrame::Anim::Tween;
using ImFrame::Easing::Linear;
using Catch::Approx;

TEST_CASE("AnimSequence Wait step completes after correct duration", "[unit]") {
    AnimSequence seq;
    seq.Wait(1.0f);

    seq.Update(0.9f);
    REQUIRE_FALSE(seq.IsDone());

    seq.Update(0.2f);
    REQUIRE(seq.IsDone());
}

TEST_CASE("AnimSequence Call fires at correct step", "[unit]") {
    int fired = 0;
    AnimSequence seq;
    seq.Call([&fired] { ++fired; });

    REQUIRE(fired == 0);
    seq.Update(0.0f);
    REQUIRE(fired == 1);
    REQUIRE(seq.IsDone());
}

TEST_CASE("AnimSequence multiple Call steps fire in order", "[unit]") {
    int last = 0;
    AnimSequence seq;
    seq.Call([&last] { last = 1; });
    seq.Call([&last] { last = 2; });
    seq.Call([&last] { last = 3; });

    seq.Update(0.0f);   // step 0: last=1, advance
    REQUIRE(last == 1);
    REQUIRE_FALSE(seq.IsDone());

    seq.Update(0.0f);   // step 1: last=2, advance
    REQUIRE(last == 2);

    seq.Update(0.0f);   // step 2: last=3, done
    REQUIRE(last == 3);
    REQUIRE(seq.IsDone());
}

TEST_CASE("AnimSequence Then drives a tween until it completes", "[unit]") {
    Tween<float> tw(0.0f, 1.0f, 0.5f, Linear);
    AnimSequence seq;
    seq.Then(tw);

    // Tween needs 0.5 s; update 0.3 s — should not be done
    seq.Update(0.3f);
    REQUIRE_FALSE(seq.IsDone());
    REQUIRE(tw.RawProgress() == Approx(0.3f / 0.5f).epsilon(0.01f));

    // Update 0.3 s more — tween crosses 0.5 s, sequence completes
    seq.Update(0.3f);
    REQUIRE(seq.IsDone());
    REQUIRE(tw.IsDone());
}

TEST_CASE("AnimSequence Loop restarts after completion", "[unit]") {
    int callCount = 0;
    AnimSequence seq;
    seq.Wait(0.1f);
    seq.Call([&callCount] { ++callCount; });
    seq.Loop(true);

    // First pass
    seq.Update(0.1f);   // Wait completes
    seq.Update(0.0f);   // Call fires → callCount=1, loop back to start
    REQUIRE(callCount == 1);
    REQUIRE_FALSE(seq.IsDone());  // looping, never done

    // Second pass
    seq.Update(0.1f);   // Wait completes again
    seq.Update(0.0f);   // Call fires → callCount=2
    REQUIRE(callCount == 2);
    REQUIRE_FALSE(seq.IsDone());
}
