/**
 * @file     State.hpp
 * @brief    Typed mutable state container for stateful `Component`-satisfying classes
 *
 * Declare a `State<T>` as a member of any `Component`-satisfying type to give it
 * persistent, mutable data that survives across frames. `T` must be a
 * default-constructible aggregate. Call `Get()` to read the value and `Set()` to
 * apply a mutation.
 *
 * **State lifecycle**
 * - `State<T>` stores its data in a shared control block (`shared_ptr<Node>`).
 *   Copies of a `State<T>` share the same block, so the component value stored
 *   inside a `ComponentElement` and the original component at the call site both
 *   read/write the same data.
 * - The block is destroyed when the last `State<T>` copy is destroyed — i.e. when
 *   the component is unmounted and no other holder (e.g. a captured lambda) remains.
 *
 * **Dirty registration**
 * `Get()` refreshes the owning element's "mark dirty" callback every time it is
 * called during a `Build()` pass (`g_stateRegistrar != nullptr`). After registration,
 * any `Set()` call fires that callback, scheduling a subtree rebuild for the next
 * frame. The rebuild is naturally coalesced: the element's dirty flag is an
 * `atomic<bool>`, so N `Set()` calls → N fires → one rebuild.
 *
 * **Thread safety**
 * `Set()` must be called on the render thread. For mutations from background threads
 * use `Tree::Signal<T>`, which wraps its value in a mutex.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-02
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Context.hpp"

#include <functional>
#include <memory>

namespace ImFrame::Tree {

/**
 * @class    State<T>
 * @brief    Typed mutable state container owned by a stateful component
 *
 * @tparam T  Default-constructible aggregate type for the state data.
 *
 * @since  2.3.0
 *
 * @example
 * @code
 * struct Counter {
 *     struct S { int count = 0; };
 *     State<S> state;
 *
 *     Widget Build() const {
 *         return GestureRegion()
 *             .OnClick([s = state]() { s.Set([](S& st) { st.count++; }); })
 *             .Child(Text(std::format("Count: {}", state.Get().count)));
 *     }
 * };
 * @endcode
 */
template <typename T>
class State {
public:
    /// Constructs a State with a default-constructed `T`.
    State() : _node(std::make_shared<Node>()) {}

    /**
     * @brief    Read the current state value.
     *
     * If called during a `Component::Build()` pass, refreshes the owning element's
     * dirty callback so that a subsequent `Set()` will trigger a rebuild. Safe to
     * call outside `Build()` for read-only access (no callback registered then).
     *
     * @return   Const reference to the current data.
     */
    [[nodiscard]] const T& Get() const noexcept {
        if (Internal::g_stateRegistrar) {
            _node->onDirty = *Internal::g_stateRegistrar;
        }
        return _node->data;
    }

    /**
     * @brief    Apply a mutation to the state and schedule a subtree rebuild.
     *
     * `mutator` is invoked synchronously with a mutable reference to the data.
     * After it returns, the owning element's dirty flag is set (if registered),
     * scheduling one rebuild the next frame regardless of how many `Set()` calls
     * occur within the same frame.
     *
     * `const` on `State<T>` — mutation targets the shared Node, not the handle.
     * Safe to call from a captured-by-value `State<T>` inside a const lambda.
     *
     * Must be called on the render thread. Use `Tree::Signal<T>` for background
     * thread mutations.
     *
     * @param[in] mutator  Callable receiving `T&`; applies the desired mutation.
     */
    void Set(std::function<void(T&)> mutator) const {
        mutator(_node->data);
        if (_node->onDirty) { _node->onDirty(); }
    }

private:
    struct Node {
        T                     data{};
        std::function<void()> onDirty;
    };

    std::shared_ptr<Node> _node;
};

} // namespace ImFrame::Tree
