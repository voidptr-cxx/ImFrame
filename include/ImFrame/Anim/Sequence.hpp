/**
 * @file     Sequence.hpp
 * @brief    Sequential animation chain executing steps one at a time
 *
 * `AnimSequence` builds a list of animation steps and executes them in order.
 * Each step is a callable returning `bool` — `true` when complete. Steps are
 * added with `Then`, `Wait`, and `Call`. Call `Update(dt)` each frame to
 * advance the current step.
 *
 * Steps hold references to their associated `Tween<T>` objects — the caller
 * is responsible for keeping tweens alive for the lifetime of the sequence.
 *
 * `AnimSequence` is not thread-safe. Update only from the render thread.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Anim/Tween.hpp"

#include <functional>
#include <vector>

namespace ImFrame::Anim {

/**
 * @brief Executes animation steps in order, advancing one step per `Update` call
 *
 * @since 1.2.0
 *
 * @example
 * @code
 * using ImFrame::Anim::AnimSequence;
 * using ImFrame::Anim::Tween;
 * Tween<float> slide(0.0f, 200.0f, 0.4f);
 * AnimSequence seq;
 * seq.Then(slide).Wait(0.1f).Call([](){ printf("done\n"); });
 * // each frame:
 * seq.Update(dt);
 * @endcode
 */
class AnimSequence {
public:
    // ─── Step builders ────────────────────────────────────────────────────────

    /**
     * @brief Adds a step that restarts and drives a tween until it completes
     * @param[in] tween Tween to play — must outlive this sequence step
     * @return Reference to this sequence for chaining
     */
    template<Lerpable T>
    AnimSequence& Then(Tween<T>& tween) {
        _steps.emplace_back([&tween, started = false](float dt) mutable -> bool {
            if (!started) {
                tween.Restart();
                started = true;
            }
            tween.Update(dt);
            return tween.IsDone();
        });
        return *this;
    }

    /**
     * @brief Adds a step that waits for the specified duration
     * @param[in] seconds Duration to wait in seconds
     * @return Reference to this sequence for chaining
     */
    AnimSequence& Wait(float seconds) {
        _steps.emplace_back([seconds, elapsed = 0.0f](float dt) mutable -> bool {
            elapsed += dt;
            return elapsed >= seconds;
        });
        return *this;
    }

    /**
     * @brief Adds an instant step that invokes a callback and immediately advances
     * @param[in] fn Callable invoked when this step is reached
     * @return Reference to this sequence for chaining
     */
    AnimSequence& Call(std::function<void()> fn) {
        _steps.emplace_back([fn = std::move(fn)](float) -> bool {
            fn();
            return true;
        });
        return *this;
    }

    /**
     * @brief Controls whether the sequence loops back to the start on completion
     * @param[in] loop true to repeat indefinitely, false to stop after one pass
     * @return Reference to this sequence for chaining
     */
    AnimSequence& Loop(bool loop = true) {
        _loop = loop;
        return *this;
    }

    // ─── Per-frame update ─────────────────────────────────────────────────────

    /**
     * @brief Advances the current step; moves to the next when the step completes
     * @param[in] dt Delta time in seconds since the last frame
     */
    void Update(float dt) {
        if (_done || _steps.empty()) return;
        if (_steps[_current](dt)) {
            ++_current;
            if (_current >= _steps.size()) {
                if (_loop) {
                    _current = 0;
                } else {
                    _done = true;
                }
            }
        }
    }

    /**
     * @brief Returns true after all steps have completed (only meaningful when not looping)
     * @return true if the sequence has finished and is not set to loop
     */
    [[nodiscard]] bool IsDone() const { return _done; }

private:
    std::vector<std::function<bool(float)>> _steps;
    std::size_t _current = 0;
    bool        _loop    = false;
    bool        _done    = false;
};

} // namespace ImFrame::Anim
