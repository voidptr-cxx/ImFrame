/**
 * @file     Signal.hpp
 * @brief    Thread-safe reactive value cell for the widget tree state layer
 *
 * `Tree::Signal<T>` is a typed reactive VALUE cell — distinct from
 * `Utility::Signal<void(Args...)>`, which is an event observable. A
 * `Tree::Signal<T>` stores a single value of type `T`, notifies registered
 * build-element callbacks when it changes, and also supports permanent
 * subscriptions (for use by `Computed<T>`).
 *
 * **Difference from `State<T>`**
 * `State<T>` is render-thread-only. `Tree::Signal<T>` wraps its value in a
 * mutex, so `operator=()` is safe to call from any thread — useful for
 * posting results from a background worker into the UI.
 *
 * **Build-time dirty registration**
 * Calling `Get()` during a `Component::Build()` pass (while
 * `Internal::g_stateRegistrar` is set) registers the calling element's
 * mark-dirty callback as `onDirty`. On `operator=()`, that callback fires
 * to schedule a subtree rebuild for the next frame.
 *
 * **Permanent subscriptions**
 * `OnChange()` returns a `Utility::Connection` that stays alive until
 * explicitly disconnected or dropped. Used by `Computed<T>` to track
 * which `Signal`s feed into a derived computation.
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
#include "ImFrame/Utility/Signal.hpp"

#include <functional>
#include <memory>
#include <mutex>

namespace ImFrame::Tree {

/**
 * @class    Signal<T>
 * @brief    Thread-safe reactive value cell that notifies dependents on assignment
 *
 * Copies of a `Signal<T>` share the same internal control block, so assignment
 * (`operator=`) via any copy notifies all registered watchers.
 *
 * @tparam   T  Value type. Must be copy-constructible.
 * @since    2.3.0
 *
 * @example
 * @code
 * struct Worker {
 *     Tree::Signal<float> progress;
 *
 *     void RunOnBackground() {
 *         for (int i = 0; i <= 100; ++i) {
 *             progress = static_cast<float>(i) / 100.f;
 *         }
 *     }
 *
 *     Widget Build() const {
 *         return ProgressBar(progress.Get());
 *     }
 * };
 * @endcode
 */
template <typename T>
class Signal {
public:
    /// Constructs a Signal with a default-constructed value.
    Signal() : _node(std::make_shared<Node>()) {}

    /// Constructs a Signal with the given initial value.
    explicit Signal(T initial) : _node(std::make_shared<Node>(std::move(initial))) {}

    /**
     * @brief    Read the current value.
     *
     * If called during a `Component::Build()` pass, registers the owning element's
     * dirty callback so that a subsequent assignment triggers a rebuild. Thread-safe.
     *
     * @return   Current value (copy, taken under mutex).
     */
    [[nodiscard]] T Get() const {
        std::lock_guard lock(_node->mutex);
        if (Internal::g_stateRegistrar) {
            _node->onDirty = *Internal::g_stateRegistrar;
        }
        return _node->value;
    }

    /**
     * @brief    Assign a new value and notify all registered dependents.
     *
     * Safe to call from any thread. Stores the value under the mutex, then
     * fires the single build-element dirty callback (if set) and emits the
     * permanent change signal (for `Computed<T>` subscribers).
     *
     * `const` on `Signal<T>` — mutation targets the shared Node, not the handle.
     *
     * @param[in] newValue  Replacement value.
     */
    void operator=(T newValue) const {
        std::function<void()> dirty;
        {
            std::lock_guard lock(_node->mutex);
            _node->value  = std::move(newValue);
            dirty         = _node->onDirty;
        }
        if (dirty) { dirty(); }
        _node->changeSignal.Emit();
    }

    /**
     * @brief    Subscribe a permanent callback fired on every assignment.
     *
     * Primarily used by `Computed<T>` to invalidate its cache when a depended
     * `Signal` changes. The returned `Utility::Connection` must be stored —
     * dropping it disconnects the subscription immediately.
     *
     * @param[in] fn  Callable invoked on every `operator=()` call.
     * @return   RAII connection handle.
     */
    [[nodiscard]] Utility::Connection OnChange(std::function<void()> fn) const {
        return _node->changeSignal.Connect(std::move(fn));
    }

private:
    struct Node {
        mutable std::mutex                     mutex;
        T                                      value{};
        std::function<void()>                  onDirty;
        Utility::ThreadSafeSignal<void()>      changeSignal;

        Node() = default;
        explicit Node(T v) : value(std::move(v)) {}
    };

    std::shared_ptr<Node> _node;
};

} // namespace ImFrame::Tree
