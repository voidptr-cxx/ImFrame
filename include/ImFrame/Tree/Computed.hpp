/**
 * @file     Computed.hpp
 * @brief    Lazily-evaluated derived value with automatic dependency tracking
 *
 * `Computed<T>` wraps a callable that computes a `T` from one or more
 * `Tree::Signal` dependencies. The value is re-computed lazily — only when
 * `Value()` is called after one or more dependencies have changed.
 *
 * **Dependency wiring**
 * Dependencies are declared at construction time as `Tree::Signal<U>` arguments.
 * Internally, `Computed` calls `dep.OnChange()` for each dep and stores the
 * resulting `Utility::Connection`s. When any dep fires, the `Computed` marks
 * itself dirty and, if registered with a building element, marks that element
 * dirty too.
 *
 * **Build-time integration**
 * If `Value()` is called during a `Component::Build()` pass
 * (`Internal::g_stateRegistrar` set), the calling element's mark-dirty callback
 * is stored. A subsequent dep change then cascades through:
 * dep Signal → Computed dirty → element dirty → rebuild next frame.
 *
 * **Thread safety**
 * `OnChange()` subscriptions fire on whatever thread wrote to the Signal (see
 * `Signal::operator=()`). `Value()` itself must be called on the render thread.
 * The `onDirty` callback is protected by a mutex.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-02
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Tree/Signal.hpp"
#include "ImFrame/Utility/Signal.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace ImFrame::Tree {

/**
 * @class    Computed<T>
 * @brief    Derived reactive value with lazy re-evaluation and dep tracking
 *
 * @tparam   T  Return type of the computation function. Must be copy-constructible.
 * @since    2.3.0
 *
 * @example
 * @code
 * Tree::Signal<int>    width{800};
 * Tree::Signal<int>    height{600};
 * Tree::Computed<long> area{[&]{ return (long)width.Get() * height.Get(); },
 *                            width, height};
 *
 * // area.Value() == 480000
 * width = 1024;
 * // area.Value() == 614400  (lazily recomputed)
 * @endcode
 */
template <typename T>
class Computed {
public:
    /**
     * @brief    Constructs a Computed with a function and zero or more Signal deps.
     *
     * Subscribes to each dep via `Signal::OnChange()`. Performs an initial
     * evaluation to populate the cache.
     *
     * @tparam   Fn    Callable returning `T`.
     * @tparam   Deps  Pack of `Tree::Signal<U>` types.
     * @param[in] fn    The computation function. Captured by value.
     * @param[in] deps  Signal dependencies. Held by const reference for construction only.
     */
    template <typename Fn, typename... Deps>
    explicit Computed(Fn fn, const Deps&... deps)
        : _node(std::make_shared<Node>(std::move(fn))) {
        auto markDirty = [nodeWeak = std::weak_ptr<Node>(_node)]() {
            if (auto n = nodeWeak.lock()) {
                n->dirty.store(true, std::memory_order_release);
                std::function<void()> cb;
                {
                    std::lock_guard lock(n->onDirtyMutex);
                    cb = n->onDirty;
                }
                if (cb) { cb(); }
            }
        };
        _node->connections.reserve(sizeof...(Deps));
        (_node->connections.push_back(deps.OnChange(markDirty)), ...);
        _node->cachedValue = _node->fn();
        _node->dirty.store(false, std::memory_order_release);
    }

    /**
     * @brief    Returns the current (possibly recomputed) derived value.
     *
     * If one or more dependencies changed since the last call, the computation
     * function is re-invoked and the result cached. If called during a
     * `Component::Build()` pass, registers the building element as a dependent
     * so it is marked dirty on the next dep change.
     *
     * @return   Const reference to the cached value.
     */
    [[nodiscard]] const T& Value() const {
        if (_node->dirty.exchange(false, std::memory_order_acq_rel)) {
            _node->cachedValue = _node->fn();
        }
        if (Internal::g_stateRegistrar) {
            std::lock_guard lock(_node->onDirtyMutex);
            _node->onDirty = *Internal::g_stateRegistrar;
        }
        return _node->cachedValue;
    }

private:
    struct Node {
        std::function<T()>               fn;
        mutable T                        cachedValue{};
        std::atomic<bool>                dirty{true};
        mutable std::mutex               onDirtyMutex;
        std::function<void()>            onDirty;
        std::vector<Utility::Connection> connections;

        explicit Node(std::function<T()> f) : fn(std::move(f)) {}
    };

    std::shared_ptr<Node> _node;
};

} // namespace ImFrame::Tree
