/**
 * @file     Context.hpp
 * @brief    Build-time context for `Component::Build()` — inherited-value lookup and state registration
 *
 * Provides two compile-time mechanisms wired into every `ComponentElement<T>::Rebuild()` pass:
 *
 *  1. **State/Signal registration** (`g_stateRegistrar` / `StateRegistrarScope`): before
 *     calling `Build()`, the element pushes its "mark-dirty" callback via a
 *     `StateRegistrarScope`. `State<T>::Get()` and `Tree::Signal<T>::Get()` pick up this
 *     callback the first time they are read in a `Build()` pass and store it so that a
 *     later `Set()`/`operator=()` can notify the element to rebuild.
 *
 *  2. **Element walk** (`g_currentBuildingElement` / `BuildElementScope`): the element
 *     registers itself as the currently-building element. `Context::Of<T>()` walks the
 *     `_parent` chain from this element upward to find the nearest
 *     `InheritedElement<T>` ancestor, then registers the element as a dependent so it
 *     is invalidated when the inherited value changes.
 *
 * Both thread-locals are restored to their previous values on scope exit via RAII, so
 * nested `Rebuild()` calls (e.g. within `ComponentElement<T>` child recursion) work
 * correctly.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-02
 * @version  2.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Tree/Element.hpp"

#include <functional>
#include <typeindex>

namespace ImFrame::Internal {

// ─── Thread-locals ────────────────────────────────────────────────────────────

/**
 * @brief  Pointer to the "mark dirty" callback of the ComponentElement currently running Rebuild().
 *
 * Set by `StateRegistrarScope`; read by `State<T>::Get()` and `Tree::Signal<T>::Get()`
 * to lazily bind a dirty-notification path.  Always `nullptr` between frames.
 */
inline thread_local std::function<void()>* g_stateRegistrar = nullptr;

/**
 * @brief  Pointer to the ComponentElement currently executing `Build()`.
 *
 * Set by `BuildElementScope`; read by `Context::Of<T>()` to start the upward
 * InheritedWidget<T> walk.  Always `nullptr` between frames.
 */
inline thread_local Tree::Element* g_currentBuildingElement = nullptr;

// ─── StateRegistrarScope ──────────────────────────────────────────────────────

/**
 * @struct StateRegistrarScope
 * @brief  RAII guard: installs `fn` as the dirty-notify callback for the current Build() pass
 *
 * Usage in `ComponentElement<T>::Rebuild()`:
 * @code
 * std::function<void()> markDirty = [flag = _dirtyFlag]() {
 *     flag->store(true, std::memory_order_relaxed);
 * };
 * StateRegistrarScope scope{markDirty};
 * Tree::Widget built = _component.Build();
 * @endcode
 *
 * @since  2.3.0
 */
struct StateRegistrarScope {
    explicit StateRegistrarScope(std::function<void()>& fn) noexcept {
        _prev             = g_stateRegistrar;
        g_stateRegistrar  = &fn;
    }
    ~StateRegistrarScope() noexcept { g_stateRegistrar = _prev; }

    StateRegistrarScope(const StateRegistrarScope&)            = delete;
    StateRegistrarScope& operator=(const StateRegistrarScope&) = delete;

private:
    std::function<void()>* _prev;
};

// ─── BuildElementScope ────────────────────────────────────────────────────────

/**
 * @struct BuildElementScope
 * @brief  RAII guard: registers `elem` as the currently-building element for Context::Of<T>()
 *
 * @since  2.3.0
 */
struct BuildElementScope {
    explicit BuildElementScope(Tree::Element* elem) noexcept {
        _prev                    = g_currentBuildingElement;
        g_currentBuildingElement = elem;
    }
    ~BuildElementScope() noexcept { g_currentBuildingElement = _prev; }

    BuildElementScope(const BuildElementScope&)            = delete;
    BuildElementScope& operator=(const BuildElementScope&) = delete;

private:
    Tree::Element* _prev;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree {

// ─── Context ──────────────────────────────────────────────────────────────────

/**
 * @class    Context
 * @brief    Provides inherited-value lookup for use inside `Component::Build()`
 *
 * `Context` is a pure-static utility class. All methods are thread-local aware and
 * are only meaningful when called during a `Component::Build()` pass (i.e. while a
 * `BuildElementScope` is active).
 *
 * @since    2.3.0
 *
 * @example
 * @code
 * struct ThemeConsumer {
 *     Widget Build() const {
 *         if (auto* theme = Context::Of<MyTheme>()) {
 *             return Text(theme->name);
 *         }
 *         return Text("(no theme)");
 *     }
 * };
 * @endcode
 */
class Context {
public:
    Context() = delete;

    /**
     * @brief    Locate the nearest `InheritedWidget<T>` ancestor value
     *
     * Walks the element tree from the currently-building element upward, looking for
     * an `InheritedElement<T>` whose `GetInheritedValue(typeid(T))` is non-null.
     * When found, also registers the calling element as a dependent so it is marked
     * dirty if the inherited value changes in a later frame.
     *
     * Returns `nullptr` if called outside a `Build()` pass or if no ancestor
     * provides `T`.
     *
     * @tparam   T  The value type to look up.
     * @return   Pointer to the nearest ancestor's `T` value, or `nullptr`.
     */
    template <typename T>
    [[nodiscard]] static const T* Of() noexcept {
        Element* elem = Internal::g_currentBuildingElement;
        while (elem) {
            if (const void* ptr = elem->GetInheritedValue(typeid(T))) {
                if (Internal::g_stateRegistrar) {
                    elem->RegisterDependentDirtyCallback(*Internal::g_stateRegistrar);
                }
                return static_cast<const T*>(ptr);
            }
            elem = elem->Parent();
        }
        return nullptr;
    }

    /**
     * @brief    Returns the element currently executing `Build()`, or `nullptr`.
     *
     * Useful for advanced tooling; not needed in typical component code.
     *
     * @return   Raw pointer to the building element.
     */
    [[nodiscard]] static Element* CurrentElement() noexcept {
        return Internal::g_currentBuildingElement;
    }
};

} // namespace ImFrame::Tree
