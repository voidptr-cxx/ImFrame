/**
 * @file     Key.hpp
 * @brief    Explicit identity override for widget tree reconciliation
 *
 * `Key` lets a widget opt out of structural (index-based) identity. Without an
 * explicit key, an `Element` at child index 3 that moves to index 7 between
 * frames appears to the reconciler as "child 3 destroyed, child 7 created" —
 * losing all element state. A shared `Key` value across both frames preserves
 * identity through the move.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <cstdint>

namespace ImFrame::Tree {

/**
 * @class    Key
 * @brief    Optional explicit identity value attached to a `Widget`
 *
 * A default-constructed `Key` has no value — the owning widget falls back to
 * structural identity (its index within its parent's child list). Two `Key`
 * values compare equal only when both carry a value and that value matches.
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * // Assign each list-item widget the entity's stable id so a reorder
 * // doesn't destroy and recreate its Element.
 * ListItem(entity.name).Key(Tree::Key{entity.id})
 * @endcode
 */
class Key {
public:
    /// Constructs a `Key` with no value — falls back to structural identity.
    constexpr Key() noexcept = default;

    /// Constructs a `Key` carrying an explicit identity value.
    constexpr explicit Key(std::uint64_t value) noexcept : _value(value), _hasValue(true) {}

    [[nodiscard]] constexpr bool HasValue() const noexcept { return _hasValue; }
    [[nodiscard]] constexpr std::uint64_t Value() const noexcept { return _value; }

    [[nodiscard]] friend constexpr bool operator==(const Key& lhs, const Key& rhs) noexcept {
        return lhs._hasValue == rhs._hasValue && (!lhs._hasValue || lhs._value == rhs._value);
    }

private:
    std::uint64_t _value    = 0;
    bool          _hasValue = false;
};

} // namespace ImFrame::Tree
