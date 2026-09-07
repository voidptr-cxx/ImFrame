/**
 * @file     Component.hpp
 * @brief    Concept satisfied by any user-defined type with a `Build()` method
 *
 * `Component` is not a base class to inherit from — it is a structural concept.
 * Any type with a `Build() const` method returning something convertible to a
 * `Widget` qualifies. The eight `Tree::Primitives` types are the only widgets
 * that do *not* satisfy `Component` (they implement `CreateElement()` directly
 * instead — see `Widget.hpp`'s `PrimitiveWidget` concept).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace ImFrame::Tree {

/**
 * @concept  Component
 * @brief    Satisfied by any type exposing `Build() const` returning a non-void value
 *
 * Deliberately does not require the return type to be `Widget` by name — that
 * would require `Widget` to be a complete type wherever `Component` is
 * evaluated, including in headers that only need the concept's existence
 * check. `Widget`'s own templated constructor performs the precise
 * `std::convertible_to<Widget>` check once `Widget` is complete.
 *
 * @since    2.2.0
 *
 * @example
 * @code
 * struct MyButton {
 *     std::string_view label;
 *     Widget Build() const {
 *         return Box().Padding(EdgeInsets::All(8.0f)).Child(Text(label));
 *     }
 * };
 * static_assert(Component<MyButton>);
 * @endcode
 */
template <typename T>
concept Component = requires(const T& t) {
    { t.Build() };
} && !std::is_void_v<decltype(std::declval<const T&>().Build())>;

} // namespace ImFrame::Tree
