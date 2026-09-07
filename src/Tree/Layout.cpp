/**
 * @file     Layout.cpp
 * @brief    Implementation of the pure flex-layout math
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Layout.hpp"

#include <algorithm>
#include <numeric>

namespace ImFrame::Internal {

std::vector<float> DistributeFlexFactors(float available, float gap,
                                          const std::vector<int>& factors,
                                          const std::vector<float>& fixedSizes) {
    const std::size_t  n = factors.size();
    std::vector<float> result(n, 0.0f);

    int   totalFactor = 0;
    float fixedSum     = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        if (factors[i] > 0) {
            totalFactor += factors[i];
        } else {
            fixedSum  += fixedSizes[i];
            result[i]  = fixedSizes[i];
        }
    }

    const float gapsTotal = (n > 1) ? gap * static_cast<float>(n - 1) : 0.0f;
    const float remaining = std::max(0.0f, available - fixedSum - gapsTotal);

    if (totalFactor > 0) {
        for (std::size_t i = 0; i < n; ++i) {
            if (factors[i] > 0) {
                result[i] = remaining * (static_cast<float>(factors[i]) / static_cast<float>(totalFactor));
            }
        }
    }
    return result;
}

std::vector<float> ComputeMainAxisOffsets(FlexMainAlign align, float available, float gap,
                                           const std::vector<float>& sizes) {
    const std::size_t  n = sizes.size();
    std::vector<float> offsets(n, 0.0f);
    if (n == 0) { return offsets; }

    const float contentSum = std::accumulate(sizes.begin(), sizes.end(), 0.0f);
    const float gapsTotal  = (n > 1) ? gap * static_cast<float>(n - 1) : 0.0f;
    const float freeSpace  = std::max(0.0f, available - contentSum - gapsTotal);

    switch (align) {
    case FlexMainAlign::Start: {
        float cursor = 0.0f;
        for (std::size_t i = 0; i < n; ++i) { offsets[i] = cursor; cursor += sizes[i] + gap; }
        break;
    }
    case FlexMainAlign::Center: {
        float cursor = freeSpace * 0.5f;
        for (std::size_t i = 0; i < n; ++i) { offsets[i] = cursor; cursor += sizes[i] + gap; }
        break;
    }
    case FlexMainAlign::End: {
        float cursor = freeSpace;
        for (std::size_t i = 0; i < n; ++i) { offsets[i] = cursor; cursor += sizes[i] + gap; }
        break;
    }
    case FlexMainAlign::SpaceBetween: {
        if (n == 1) { offsets[0] = 0.0f; break; }
        const float spacing = freeSpace / static_cast<float>(n - 1);
        float       cursor  = 0.0f;
        for (std::size_t i = 0; i < n; ++i) { offsets[i] = cursor; cursor += sizes[i] + spacing; }
        break;
    }
    case FlexMainAlign::SpaceAround: {
        const float spacing = freeSpace / static_cast<float>(n);
        float       cursor  = spacing * 0.5f;
        for (std::size_t i = 0; i < n; ++i) { offsets[i] = cursor; cursor += sizes[i] + spacing; }
        break;
    }
    }
    return offsets;
}

float ComputeCrossAxisOffset(FlexCrossAlign align, float crossSize, float childCrossSize) noexcept {
    switch (align) {
    case FlexCrossAlign::Start:   return 0.0f;
    case FlexCrossAlign::Center:  return (crossSize - childCrossSize) * 0.5f;
    case FlexCrossAlign::End:     return crossSize - childCrossSize;
    case FlexCrossAlign::Stretch: return 0.0f;
    }
    return 0.0f;
}

} // namespace ImFrame::Internal
