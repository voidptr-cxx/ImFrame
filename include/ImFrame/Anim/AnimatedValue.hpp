/**
 * @file     AnimatedValue.hpp
 * @brief    Frame-rate-independent exponential-decay value tracker
 *
 * `AnimatedValue<T>` smoothly tracks a changing target using an exponential
 * decay formula:
 *
 *     current = current + (target - current) * (1 - exp(-speed * dt))
 *
 * This formula is mathematically frame-rate independent — updating with one
 * large dt gives the same result as many small dt steps summing to the same
 * total. It is ideal for hover colour transitions, button states, and any
 * case where the target changes frequently and smooth tracking matters more
 * than precise timing.
 *
 * `AnimatedValue<T>` is not thread-safe. Update only from the render thread.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-08
 * @version  1.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Anim/Tween.hpp"

#include <cmath>

namespace ImFrame::Anim {

/**
 * @brief Smooth exponential-decay tracker toward a target value
 *
 * Speed 1 takes ~2 s to reach 95% of the target. Speed 4 takes ~0.5 s.
 * Speed 8 takes ~0.25 s. Speed 10 takes ~0.2 s.
 *
 * @tparam T Any `Lerpable` type
 *
 * @since 1.2.0
 *
 * @example
 * @code
 * using ImFrame::Anim::AnimatedValue;
 * AnimatedValue<float> opacity(0.0f, 8.0f);
 * opacity.SetTarget(1.0f);         // on hover
 * float alpha = opacity.Update(dt); // each frame
 * @endcode
 */
template<Lerpable T>
class AnimatedValue {
public:
    // ─── Construction ─────────────────────────────────────────────────────────

    /**
     * @brief Constructs an animated value at an initial position
     * @param[in] initial Starting value (both current and target)
     * @param[in] speed   Decay rate — higher values track faster (default 8)
     * @throws Nothing — noexcept
     */
    explicit AnimatedValue(T initial, float speed = 8.0f)
        : _current(initial), _target(initial), _speed(speed)
    {}

    // ─── Target control ───────────────────────────────────────────────────────

    /**
     * @brief Sets the value to track toward
     * @param[in] target New target value
     */
    void SetTarget(T target) { _target = std::move(target); }

    // ─── Per-frame update ─────────────────────────────────────────────────────

    /**
     * @brief Advances the value toward the target and returns the current value
     *
     * Applies `current = current + (target - current) * (1 - exp(-speed * dt))`.
     *
     * @param[in] dt Delta time in seconds since the last frame
     * @return Current smoothed value after the update
     */
    T Update(float dt) {
        const float alpha = 1.0f - std::exp(-_speed * dt);
        _current = _current + (_target - _current) * alpha;
        return _current;
    }

    // ─── Queries ──────────────────────────────────────────────────────────────

    /**
     * @brief Returns the current value without advancing the simulation
     * @return Current smoothed value
     */
    [[nodiscard]] T Value() const { return _current; }

    /// Instantly sets current equal to target with no transition
    void SnapToTarget() { _current = _target; }

private:
    T     _current;
    T     _target;
    float _speed;
};

} // namespace ImFrame::Anim
