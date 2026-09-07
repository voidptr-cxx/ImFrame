/**
 * @file     Widget.hpp
 * @brief    Immutable, type-erased description node for the declarative widget tree
 *
 * `Widget` is the user-facing value type returned by every `Component::Build()`
 * and accepted by every primitive's `Child()`/`Children()` setter. It carries
 * no live state of its own — `CreateElement()` produces the corresponding
 * stateful `Element` the first time a `Widget` appears at a tree position.
 *
 * Two kinds of concrete type can be wrapped into a `Widget`:
 *  - **Primitive widgets** (the eight `Tree::Primitives` types) implement
 *    `CreateElement() const -> std::unique_ptr<Element>` directly. Their
 *    `Element` owns layout/paint logic in `ImFrame::Internal::`.
 *  - **Components** (`Component` concept — any type with `Build() const`)
 *    are wrapped in a generic `ComponentElement<T>` that calls `T::Build()`
 *    each time it needs to (re)produce its single child.
 *
 * @internal
 * Implementation note (see `DECISIONS.md`, 2026-06-30): true zero-allocation
 * heterogeneous trees are not achievable in standard C++ without a closed
 * variant of every possible widget type, which would make `Component`
 * impossible for downstream code to extend. `Widget` therefore wraps a
 * `std::shared_ptr<const WidgetConcept>` — one allocation per `Widget` node
 * at `Build()` time; copies are refcount bumps, matching the cost profile of
 * `std::function`'s type erasure.
 *
 * The type-erasure machinery (`WidgetConcept`, `PrimitiveModel<T>`,
 * `ComponentModel<T>`, `ComponentElement<T>`) lives in `ImFrame::Internal::` —
 * the project's single shared implementation-details namespace — rather than
 * a `ImFrame::Tree::Internal::` nested under this module, so that the
 * `RenderObjects/*.cpp` files (also in `ImFrame::Internal::`) can refer to
 * concrete `Element` subclasses with unqualified `Internal::` lookup without
 * a name collision between two different "Internal" namespaces.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Tree/Component.hpp"
#include "ImFrame/Tree/Context.hpp"
#include "ImFrame/Tree/Element.hpp"
#include "ImFrame/Tree/Key.hpp"

#include <atomic>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <utility>

namespace ImFrame::Tree {

class Widget;

// ─── PrimitiveWidget concept ──────────────────────────────────────────────────

/**
 * @concept  PrimitiveWidget
 * @brief    Satisfied by any of the eight `Tree::Primitives` types
 *
 * A primitive widget implements `CreateElement() const` directly, returning
 * its own concrete `Element` subclass. This is the closed set of types that
 * have a corresponding render/layout implementation; everything else is a
 * `Component`.
 *
 * @since    2.2.0
 */
template <typename T>
concept PrimitiveWidget = requires(const T& t) {
    { t.CreateElement() } -> std::same_as<std::unique_ptr<Element>>;
};

} // namespace ImFrame::Tree

namespace ImFrame::Internal {

// ─── WidgetConcept (type-erasure interface) ──────────────────────────────────

/// @internal Type-erasure interface wrapped by every `Tree::Widget` instance.
class WidgetConcept {
public:
    virtual ~WidgetConcept() = default;
    [[nodiscard]] virtual std::unique_ptr<Tree::Element> CreateElement() const = 0;
    [[nodiscard]] virtual std::type_index                TypeId() const noexcept = 0;
    [[nodiscard]] virtual Tree::Key                       GetKey() const noexcept = 0;
    /// `0` unless the wrapped type is `Expanded`/`Spacer`, in which case its flex factor.
    [[nodiscard]] virtual int                              FlexFactor() const noexcept = 0;
    /// Address of the wrapped value, for `Widget::As<T>()`. Safe only when `T` matches `TypeId()`.
    [[nodiscard]] virtual const void*                      RawValue() const noexcept = 0;
};

/// @internal Extracts `T::GetKey()` if present, else the no-value default `Key{}`.
template <typename T>
[[nodiscard]] Tree::Key ExtractKey(const T& t) noexcept {
    if constexpr (requires { { t.GetKey() } -> std::same_as<Tree::Key>; }) {
        return t.GetKey();
    } else {
        return Tree::Key{};
    }
}

/// @internal Extracts `T::GetFactor()` if present (only `Expanded`/`Spacer` define it), else `0`.
template <typename T>
[[nodiscard]] int ExtractFlexFactor(const T& t) noexcept {
    if constexpr (requires { { t.GetFactor() } -> std::same_as<int>; }) {
        return t.GetFactor();
    } else {
        return 0;
    }
}

/// @internal Wraps a primitive widget value; `CreateElement()` forwards to `T::CreateElement()`.
template <typename T>
class PrimitiveModel final : public WidgetConcept {
public:
    explicit PrimitiveModel(T value) : _value(std::move(value)) {}

    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const override { return _value.CreateElement(); }
    [[nodiscard]] std::type_index                TypeId() const noexcept override { return typeid(T); }
    [[nodiscard]] Tree::Key                       GetKey() const noexcept override { return ExtractKey(_value); }
    [[nodiscard]] int                             FlexFactor() const noexcept override { return ExtractFlexFactor(_value); }
    [[nodiscard]] const void*                     RawValue() const noexcept override { return &_value; }

private:
    T _value;
};

template <typename T>
class ComponentModel; // Defined below, after Tree::Widget/ComponentElement<T>.

} // namespace ImFrame::Internal

namespace ImFrame::Tree {

// ─── Widget ───────────────────────────────────────────────────────────────────

/**
 * @class    Widget
 * @brief    Immutable, copyable value type describing one node of a widget tree
 *
 * @since    2.2.0
 */
class Widget {
public:
    /**
     * @brief    Wraps any primitive widget or `Component`-satisfying value.
     * @tparam   T  Deduced concrete widget/component type.
     */
    template <typename T>
        requires(!std::same_as<std::remove_cvref_t<T>, Widget>) &&
                (PrimitiveWidget<std::remove_cvref_t<T>> || Component<std::remove_cvref_t<T>>)
    Widget(T widget); // NOLINT(google-explicit-constructor) — intentional implicit conversion.

    [[nodiscard]] std::unique_ptr<Element> CreateElement() const { return _impl->CreateElement(); }
    [[nodiscard]] std::type_index          TypeId() const noexcept { return _impl->TypeId(); }
    [[nodiscard]] Key                      GetKey() const noexcept { return _impl->GetKey(); }

    /// `0` unless this widget is `Expanded`/`Spacer`, in which case its flex factor. Read by `Flex`.
    [[nodiscard]] int FlexFactor() const noexcept { return _impl->FlexFactor(); }

    /// `true` when `other` describes the same concrete widget type as this widget.
    [[nodiscard]] bool CanUpdate(const Widget& other) const noexcept { return TypeId() == other.TypeId(); }

    /**
     * @brief    Recovers the concrete widget value this `Widget` wraps.
     *
     * Safe only when `T` matches the type this `Widget` was constructed from
     * — i.e. from inside a primitive's own `Element::Mount()`/`Update()`,
     * where the reconciler has already established the type match before
     * calling `Mount()`/`Update()` with this widget.
     *
     * @tparam   T  The expected concrete widget type.
     */
    template <typename T>
    [[nodiscard]] const T& As() const noexcept {
        return *static_cast<const T*>(_impl->RawValue());
    }

private:
    std::shared_ptr<const Internal::WidgetConcept> _impl;
};

// ─── Element::CanUpdate / RecordWidgetMeta (out-of-line; need Widget complete) ─

inline bool Element::CanUpdate(const Widget& newWidget) const noexcept {
    return _widgetType == newWidget.TypeId();
}

inline void Element::RecordWidgetMeta(const Widget& widget) noexcept {
    _widgetType = widget.TypeId();
    _currentKey = widget.GetKey();
}

} // namespace ImFrame::Tree

namespace ImFrame::Internal {

// ─── ComponentElement<T> ──────────────────────────────────────────────────────

/**
 * @class    ComponentElement
 * @brief    Generic `Element` for any `Component`-satisfying type `T`
 *
 * Owns one component instance and the single child `Element` produced by its
 * `Build()`. `Layout()`/`Paint()` are pure pass-through to the child — a
 * `Component` has no rendering of its own.
 *
 * **Phase 28 — dirty tracking**
 * Each `ComponentElement<T>` holds a `shared_ptr<atomic<bool>>` dirty flag.
 * `State<T>::Get()` and `Tree::Signal<T>::Get()` capture a copy of this flag
 * (via `g_stateRegistrar`) during `Build()` and atomically set it on `Set()`/
 * `operator=()`. `Update()` reads and clears the flag: if dirty, `Rebuild()` is
 * called; otherwise the last built `Widget` is forwarded to the child so the
 * child can perform its own dirty check.
 *
 * @tparam   T  A type satisfying `Tree::Component`.
 * @since    2.2.0
 */
template <typename T>
class ComponentElement final : public Tree::Element {
public:
    explicit ComponentElement(T component)
        : _component(std::move(component))
        , _dirtyFlag(std::make_shared<std::atomic<bool>>(false)) {}

    void Mount(Tree::Element* parent, std::size_t slotIndex, const Tree::Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        Rebuild();
    }

    void Update(const Tree::Widget& newWidget) override {
        _component = newWidget.As<T>();
        RecordWidgetMeta(newWidget);
        bool wasDirty = _dirtyFlag->exchange(false, std::memory_order_acq_rel);
        if (wasDirty) {
            Rebuild();
        } else if (_child && _lastBuilt.has_value()) {
            _child->Update(*_lastBuilt);
        }
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

    [[nodiscard]] std::weak_ptr<std::atomic<bool>> GetDirtyFlag() const noexcept override {
        return _dirtyFlag;
    }

private:
    void Rebuild() {
        _dirtyFlag->store(false, std::memory_order_release);
        std::function<void()> markDirty = [flag = _dirtyFlag]() {
            flag->store(true, std::memory_order_release);
        };
        StateRegistrarScope registrarScope{markDirty};
        BuildElementScope   elemScope{this};

        Tree::Widget built = _component.Build();
        _lastBuilt = built;

        if (_child && _child->CanUpdate(built)) {
            _child->Update(built);
        } else {
            if (_child) { _child->Unmount(); }
            _child = built.CreateElement();
            _child->Mount(this, 0, built);
        }
    }

    T                                   _component;
    std::unique_ptr<Tree::Element>      _child;
    std::optional<Tree::Widget>         _lastBuilt;
    std::shared_ptr<std::atomic<bool>>  _dirtyFlag;
};

// ─── ComponentModel<T> ────────────────────────────────────────────────────────

/// @internal Wraps a `Component`-satisfying value; `CreateElement()` produces a `ComponentElement<T>`.
template <typename T>
class ComponentModel final : public WidgetConcept {
public:
    explicit ComponentModel(T value) : _value(std::move(value)) {}

    [[nodiscard]] std::unique_ptr<Tree::Element> CreateElement() const override {
        return std::make_unique<ComponentElement<T>>(_value);
    }
    [[nodiscard]] std::type_index TypeId() const noexcept override { return typeid(T); }
    [[nodiscard]] Tree::Key       GetKey() const noexcept override { return ExtractKey(_value); }
    [[nodiscard]] int             FlexFactor() const noexcept override { return ExtractFlexFactor(_value); }
    [[nodiscard]] const void*     RawValue() const noexcept override { return &_value; }

private:
    T _value;
};

} // namespace ImFrame::Internal

namespace ImFrame::Tree {

// ─── Widget templated constructor (out-of-line; needs ComponentModel<T> complete) ─

template <typename T>
    requires(!std::same_as<std::remove_cvref_t<T>, Widget>) &&
            (PrimitiveWidget<std::remove_cvref_t<T>> || Component<std::remove_cvref_t<T>>)
Widget::Widget(T widget) {
    using DT = std::remove_cvref_t<T>;
    if constexpr (PrimitiveWidget<DT>) {
        _impl = std::make_shared<const Internal::PrimitiveModel<DT>>(std::forward<T>(widget));
    } else {
        _impl = std::make_shared<const Internal::ComponentModel<DT>>(std::forward<T>(widget));
    }
}

} // namespace ImFrame::Tree
