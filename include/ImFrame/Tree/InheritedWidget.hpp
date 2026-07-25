/**
 * @file     InheritedWidget.hpp
 * @brief    Scoped data provider that descendants can look up via `Context::Of<T>()`
 *
 * `InheritedWidget<T>` is a primitive widget (directly implements
 * `CreateElement()`) that wraps a value of type `T` and a single child subtree.
 * Descendant components call `Context::Of<T>()` from within their `Build()`
 * method to locate the nearest `InheritedWidget<T>` ancestor and obtain a
 * const pointer to its value.
 *
 * **Invalidation**
 * When the `T` value changes (parent rebuilds with a new `InheritedWidget<T>`),
 * `InheritedElement<T>` compares old and new values (if `T` is equality-comparable)
 * and, if different, fires all registered dependent dirty callbacks before
 * propagating the update to the child subtree.
 *
 * **Dependency registration**
 * `Context::Of<T>()` calls `Element::RegisterDependentDirtyCallback()` on the
 * found `InheritedElement<T>`. The callback points to the consuming
 * `ComponentElement`'s mark-dirty function and is cleared after each firing
 * (one-shot per Build pass, re-registered the next time Build is called).
 *
 * **Usage note**
 * `InheritedElement<T>` is a template and must live in this public header so the
 * compiler can instantiate it. It is in `ImFrame::Internal::` — an implementation
 * detail not intended for direct use by consumers.
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

#include "ImFrame/Tree/Widget.hpp"

#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <vector>

namespace ImFrame::Tree {

template <typename T>
class InheritedWidget; // Defined below; forward-declared so InheritedElement<T> can name it.

} // namespace ImFrame::Tree

namespace ImFrame::Internal {

// ─── InheritedElement<T> ──────────────────────────────────────────────────────

/**
 * @class    InheritedElement<T>
 * @brief    Stateful element backing `InheritedWidget<T>`; serves as an ancestor lookup target
 *
 * @internal
 * Not part of the public API. Instantiated by `InheritedWidget<T>::CreateElement()`.
 *
 * @tparam   T  The value type provided to descendant components.
 * @since    2.3.0
 */
template <typename T>
class InheritedElement final : public Tree::Element {
public:
    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        const auto& iw = widget.As<Tree::InheritedWidget<T>>();
        _value         = iw.GetValue();
        ReconcileChild(iw.GetChild(), /*isFirstMount=*/true);
    }

    void Update(const Tree::Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        const auto& iw         = newWidget.As<Tree::InheritedWidget<T>>();
        const T&    newValue   = iw.GetValue();

        bool changed = true;
        if constexpr (std::equality_comparable<T>) {
            changed = !(_value == newValue);
        }

        _value = newValue;

        if (changed) {
            NotifyDependents();
        }

        ReconcileChild(iw.GetChild(), /*isFirstMount=*/false);
    }

    void Unmount() override {
        if (_child) { _child->Unmount(); }
        _child.reset();
    }

    [[nodiscard]] Widgets::Vec2 Layout(Tree::BoxConstraints constraints) override {
        _size = _child ? _child->Layout(constraints) : Widgets::Vec2{};
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        if (_child) { _child->Paint(cmd, position); }
    }

    [[nodiscard]] const void* GetInheritedValue(std::type_index typeId) const noexcept override {
        if (typeId == typeid(T)) { return &_value; }
        return nullptr;
    }

    void RegisterDependentDirtyCallback(std::function<void()> callback) override {
        std::lock_guard lock(_depMutex);
        _dependents.push_back(std::move(callback));
    }

private:
    void NotifyDependents() {
        std::vector<std::function<void()>> callbacks;
        {
            std::lock_guard lock(_depMutex);
            callbacks = std::move(_dependents);
        }
        for (auto& cb : callbacks) {
            if (cb) { cb(); }
        }
    }

    void ReconcileChild(const Tree::Widget& childWidget, bool isFirstMount) {
        if (!isFirstMount && _child && _child->CanUpdate(childWidget)) {
            _child->Update(childWidget);
        } else {
            if (_child) { _child->Unmount(); }
            _child = childWidget.CreateElement();
            _child->Mount(this, 0, childWidget);
        }
    }

    T                                       _value{};
    std::unique_ptr<Tree::Element>          _child;
    mutable std::mutex                      _depMutex;
    std::vector<std::function<void()>>      _dependents;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree {

// ─── InheritedWidget<T> ───────────────────────────────────────────────────────

/**
 * @class    InheritedWidget<T>
 * @brief    Primitive widget that provides a scoped `T` value to its descendant subtree
 *
 * Wrap any subtree with `InheritedWidget<T>` to make the value available to all
 * descendant `Component::Build()` calls via `Context::Of<T>()`.
 *
 * @tparam   T  The value type to expose. Should be cheap to copy (it is stored by value
 *              inside each `InheritedWidget<T>` and compared on each `Update()`).
 * @since    2.3.0
 *
 * @example
 * @code
 * struct ThemeConsumer {
 *     Widget Build() const {
 *         if (auto* t = Context::Of<AppTheme>()) {
 *             return Text(t->fontName);
 *         }
 *         return Text("(no theme)");
 *     }
 * };
 *
 * // Wrap the root with the theme provider:
 * Widget root = InheritedWidget<AppTheme>(AppTheme{"Roboto"}, ThemeConsumer{});
 * @endcode
 */
template <typename T>
class InheritedWidget {
public:
    /**
     * @brief    Constructs an `InheritedWidget<T>` with a value and a child subtree.
     * @param[in] value  The value to expose to descendants.
     * @param[in] child  The child subtree to render beneath this provider.
     */
    InheritedWidget(T value, Widget child)
        : _value(std::move(value))
        , _child(std::move(child)) {}

    /// Returns a `unique_ptr<InheritedElement<T>>` for the reconciler.
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const {
        return std::make_unique<Internal::InheritedElement<T>>();
    }

    /// The value provided to descendants.
    [[nodiscard]] const T& GetValue() const noexcept { return _value; }

    /// The child subtree rendered beneath this provider.
    [[nodiscard]] const Widget& GetChild() const noexcept { return _child; }

private:
    T      _value;
    Widget _child;
};

} // namespace ImFrame::Tree
