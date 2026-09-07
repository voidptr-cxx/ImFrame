/**
 * @file     Tween.hpp
 * @brief    Interpolation tween from a start value to an end value over a duration
 *
 * Defines the `Lerpable` concept and the `Tween<T>` class template. Any type
 * where `a + (b - a) * t` is a valid expression for `float t` satisfies
 * `Lerpable` automatically — `float`, `double`, `ImVec2`, `ImVec4`, and any
 * user type with matching arithmetic operators qualify.
 *
 * `Tween<T>` is driven by delta time: call `Update(dt)` each frame and use
 * the returned interpolated value. Tweens are not thread-safe — update only
 * from the render thread.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Anim/Easing.hpp"

#include <algorithm>
#include <concepts>
#include <functional>
#include <utility>

namespace ImFrame::Anim {

// ─── Lerpable concept ─────────────────────────────────────────────────────────

/**
 * @brief Constrains a type that supports linear interpolation via `a + (b - a) * t`
 *
 * @tparam T The type to constrain
 * @since 1.2.0
 */
template<typename T>
concept Lerpable = requires(T a, T b, float t) {
    { a + (b - a) * t } -> std::convertible_to<T>;
};

// ─── Tween<T> ─────────────────────────────────────────────────────────────────

/**
 * @brief Interpolates from a start value to an end value over a fixed duration
 *
 * The tween is driven by delta time. Call `Play()` to start, then call
 * `Update(dt)` each frame to advance it and retrieve the current value.
 * The tween fires the `OnComplete` callback exactly once when it reaches
 * the end. All methods are safe to call in any state.
 *
 * @tparam T Any `Lerpable` type (float, double, ImVec2, ImVec4, ...)
 *
 * @since 1.2.0
 *
 * @example
 * @code
 * using ImFrame::Anim::Tween;
 * Tween<float> t(0.0f, 1.0f, 0.3f, ImFrame::Easing::EaseOutBack);
 * t.Play();
 * // each frame:
 * float alpha = t.Update(deltaTime);
 * @endcode
 */
template<Lerpable T>
class Tween {
public:
    // ─── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief Constructs a tween between two values
     * @param[in] from            Start value
     * @param[in] to              End value
     * @param[in] durationSeconds Total playback duration in seconds
     * @param[in] ease            Easing function mapping [0,1] → [0,1]
     * @throws Nothing — noexcept
     */
    explicit Tween(T from, T to, float durationSeconds,
                   std::function<float(float)> ease = ImFrame::Easing::EaseInOutQuad)
        : _from(std::move(from))
        , _to(std::move(to))
        , _duration(durationSeconds > 0.0f ? durationSeconds : 0.0001f)
        , _ease(std::move(ease))
    {}

    // ─── Playback control ─────────────────────────────────────────────────────

    /// Starts or resumes playback from the current position
    void Play() { _playing = true; }

    /// Suspends playback — elapsed time is preserved
    void Pause() { _playing = false; }

    /// Resumes playback from the current position
    void Resume() { _playing = true; }

    /**
     * @brief Swaps start and end values and plays from the mirrored position
     *
     * Allows smooth reversal mid-animation. `from` and `to` are swapped,
     * and elapsed time is mirrored so the tween continues from its current
     * visual position.
     */
    void Reverse() {
        std::swap(_from, _to);
        _elapsed    = _duration * (1.0f - _rawProgress);
        _rawProgress = 1.0f - _rawProgress;
        _playing     = true;
        _done        = false;
        _completeFired = false;
    }

    /// Resets to the start without beginning playback
    void Reset() {
        _elapsed       = 0.0f;
        _rawProgress   = 0.0f;
        _playing       = false;
        _done          = false;
        _completeFired = false;
    }

    /// Resets to the start and begins playback
    void Restart() {
        Reset();
        Play();
    }

    // ─── Per-frame update ─────────────────────────────────────────────────────

    /**
     * @brief Advances the tween by `dt` seconds and returns the interpolated value
     * @param[in] dt Delta time in seconds since the last frame
     * @return Current interpolated value; returns `to` when complete
     */
    T Update(float dt) {
        if (_playing && !_done) {
            _elapsed     = std::min(_elapsed + dt, _duration);
            _rawProgress = _elapsed / _duration;
            if (_rawProgress >= 1.0f && !_completeFired) {
                _playing       = false;
                _done          = true;
                _completeFired = true;
                if (_onComplete) _onComplete();
            }
        }
        return CurrentValue();
    }

    // ─── Callback ─────────────────────────────────────────────────────────────

    /**
     * @brief Registers a callback that fires once when the tween completes
     * @param[in] callback Callable invoked exactly once on completion
     */
    void OnComplete(std::function<void()> callback) {
        _onComplete = std::move(callback);
    }

    // ─── Queries ──────────────────────────────────────────────────────────────

    /**
     * @brief Returns the current eased progress in [0, 1] (may exceed range for overshoot easings)
     * @return Eased normalized progress
     */
    [[nodiscard]] float Progress() const { return _ease(_rawProgress); }

    /**
     * @brief Returns the raw (uneased) elapsed fraction in [0, 1]
     * @return Linear normalized progress
     */
    [[nodiscard]] float RawProgress() const { return _rawProgress; }

    /**
     * @brief Returns true while the tween is advancing
     * @return true if playing, false if paused or complete
     */
    [[nodiscard]] bool IsPlaying() const { return _playing; }

    /**
     * @brief Returns true after the tween has reached its end value
     * @return true if playback has completed
     */
    [[nodiscard]] bool IsDone() const { return _done; }

private:
    [[nodiscard]] T CurrentValue() const {
        if (_done) return _to;
        return _from + (_to - _from) * _ease(_rawProgress);
    }

    T                       _from;
    T                       _to;
    float                   _duration;
    float                   _elapsed       = 0.0f;
    float                   _rawProgress   = 0.0f;
    bool                    _playing       = false;
    bool                    _done          = false;
    bool                    _completeFired = false;
    std::function<float(float)> _ease;
    std::function<void()>       _onComplete;
};

} // namespace ImFrame::Anim
