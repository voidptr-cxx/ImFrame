/**
 * @file     Layout.hpp
 * @brief    Pure flex-layout math shared by FlexRO and ExpandedRO/SpacerRO
 *
 * Deliberately free of `Element`/ImGui dependencies so the distribution and
 * alignment algorithms are unit-testable in isolation (see
 * `Tests/Tree/Layout_test.cpp`).
 *
 * @internal
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <vector>

namespace ImFrame::Internal {

/// @internal Mirrors `Tree::Primitives::Flex::MainAlignment` — kept decoupled from the public header.
enum class FlexMainAlign : unsigned char { Start, Center, End, SpaceBetween, SpaceAround };

/// @internal Mirrors `Tree::Primitives::Flex::CrossAlignment` — kept decoupled from the public header.
enum class FlexCrossAlign : unsigned char { Start, Center, End, Stretch };

/**
 * @brief    Allocates main-axis size to each flex child.
 *
 * Children with `factors[i] == 0` keep `fixedSizes[i]` unchanged. The
 * remaining space (after fixed children and gaps) is split among children
 * with `factors[i] > 0` proportionally to their factor.
 *
 * @param[in] available   Total main-axis space offered by the parent.
 * @param[in] gap         Fixed gap inserted between every adjacent pair of children.
 * @param[in] factors     Per-child flex factor; `0` = not flexible.
 * @param[in] fixedSizes  Per-child intrinsic main-axis size (used only where `factors[i] == 0`).
 * @return    Per-child allocated main-axis size, same length/order as `factors`.
 */
[[nodiscard]] std::vector<float> DistributeFlexFactors(float available, float gap,
                                                         const std::vector<int>& factors,
                                                         const std::vector<float>& fixedSizes);

/**
 * @brief    Computes each child's main-axis offset for a given alignment mode.
 *
 * @param[in] align      Main-axis distribution mode.
 * @param[in] available  Total main-axis space offered by the parent.
 * @param[in] gap        Fixed gap; used by `Start`/`Center`/`End` only — the `Space*` modes
 *                        derive their own spacing from leftover space.
 * @param[in] sizes      Per-child final main-axis size.
 * @return    Per-child main-axis offset from the container's leading edge.
 */
[[nodiscard]] std::vector<float> ComputeMainAxisOffsets(FlexMainAlign align, float available, float gap,
                                                          const std::vector<float>& sizes);

/// Computes one child's cross-axis offset given the container's cross size and the child's own cross size.
[[nodiscard]] float ComputeCrossAxisOffset(FlexCrossAlign align, float crossSize, float childCrossSize) noexcept;

} // namespace ImFrame::Internal
