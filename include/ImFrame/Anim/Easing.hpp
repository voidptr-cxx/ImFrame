/**
 * @file     Easing.hpp
 * @brief    Standard easing function collection for animation curves
 *
 * All functions have signature `float(float t)` where `t` is in `[0, 1]`.
 * Each function maps `f(0) == 0` and `f(1) == 1` at the boundaries.
 * Overshoot easings (`EaseOutBack`, `EaseOutElastic`, `Spring`) may exceed
 * the `[0, 1]` range between the endpoints but always reach exactly 1 at t=1.
 *
 * Any user callable matching `float(float)` is valid as an easing argument
 * throughout the animation API — these are provided as convenient defaults.
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

#include <cmath>
#include <numbers>

namespace ImFrame::Easing {

/// Identity — linear interpolation, no easing applied
inline auto Linear = [](float t) -> float { return t; };

/// Accelerates from rest using a quadratic curve
inline auto EaseInQuad = [](float t) -> float { return t * t; };

/// Decelerates to rest using a quadratic curve
inline auto EaseOutQuad = [](float t) -> float { return t * (2.0f - t); };

/// Accelerates then decelerates using a quadratic curve
inline auto EaseInOutQuad = [](float t) -> float {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
};

/// Accelerates from rest using a cubic curve
inline auto EaseInCubic = [](float t) -> float { return t * t * t; };

/// Decelerates to rest using a cubic curve
inline auto EaseOutCubic = [](float t) -> float {
    const float f = t - 1.0f;
    return f * f * f + 1.0f;
};

/// Accelerates then decelerates using a cubic curve
inline auto EaseInOutCubic = [](float t) -> float {
    return t < 0.5f ? 4.0f * t * t * t
                    : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
};

/// Decelerates with a slight overshoot before settling at the target
inline auto EaseOutBack = [](float t) -> float {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float f = t - 1.0f;
    return 1.0f + c3 * f * f * f + c1 * f * f;
};

/// Decelerates with a spring-like oscillation before settling
inline auto EaseOutElastic = [](float t) -> float {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    constexpr float c4 = (2.0f * std::numbers::pi_v<float>) / 3.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
};

/// Exponential spring that oscillates and decays to rest
inline auto Spring = [](float t) -> float {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return 1.0f - std::exp(-10.0f * t)
               * std::cos(2.0f * std::numbers::pi_v<float> * 1.5f * t);
};

/// Decelerates with a multi-bounce landing effect
inline auto Bounce = [](float t) -> float {
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;
    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
};

} // namespace ImFrame::Easing
