/**
 * @file     WidgetTestDriver.hpp
 * @brief    Headless test harness for `Tree::Component` unit testing
 *
 * `WidgetTestDriver<T>` removes the Mount/Update lifecycle boilerplate every
 * `Tests/Tree/*.cpp` file otherwise hand-rolls (see `Reconciler_test.cpp`'s
 * manual `CreateElement()`/`Mount()`/`Update()` dance, duplicated across
 * every test file that needs it) by driving it once, correctly, through the
 * same public `Tree::Widget`/`Tree::Element` API `Examples/DemoApp`'s
 * `TreePanel` and the production `Internal::Reconciler` both already use.
 * Because it goes through the real `Mount()`/`Update()` path, `Tree::State<T>`
 * and dirty-flag semantics work exactly as they do in a running application —
 * no test-only reimplementation of that logic.
 *
 * @internal
 * Scope, decided during Phase 30.4 design (see `DECISIONS.md`): this
 * implements the proposal's "logic" test mode only. Two things the original
 * `WidgetTestDriver` proposal describes are **not** provided, and why:
 *
 * - **No runtime string/index "path" addressing.** `Tree::Element` (the live,
 *   mounted node) exposes no child-traversal API at all — only `Parent()` —
 *   and concrete `Element` subclasses are `Internal::`, defined in `.cpp`
 *   files with no public header. Building a generic runtime path resolver
 *   would need a new virtual hook on `Element` (a core-module change, not
 *   attempted here). Instead, `FindDescendant<Target>()` below walks the
 *   *`Widget` description* returned by `Build()` — compile-time
 *   type-directed, exactly the idiom already established in
 *   `Tests/Tree/Widgets_test.cpp` (pull the built `Widget`, `.As<T>()` into
 *   it). It only recognises the enumerated built-in `Tree::Primitives` +
 *   interactive `*Widget` types below; it does not expand `Tree::VirtualList`
 *   (unbounded/lazy rows) and cannot descend into a third-party `Component`
 *   it has no compile-time knowledge of.
 * - **No "rendering" test mode.** Neither `Internal::HeadlessBackend::
 *   ReadPixels()` (always zero-filled — Phase 19's own doc comment: "requires
 *   a GPU-backed implementation") nor `Rendering::HeadlessViewport::
 *   ReadPixels()` (only reflects whatever a test's own `OnRender` callback
 *   manually wrote) perform real ImGui rasterization today. A pixel-based
 *   assertion mode would either assert on nothing meaningful or be outright
 *   misleading, so it isn't built — deferred to whenever real headless
 *   rendering capture exists (Phase 31's command-buffer work is a candidate).
 *
 * `driver.Component()` gives direct access to the component under test for
 * state inspection/mutation — the same pattern every existing test already
 * uses (keep your own `State<T>`/handle copies in scope), just without
 * needing a separate variable: the driver owns the one instance under test.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-11
 * @version  2.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Tree/Element.hpp"
#include "ImFrame/Tree/Portal.hpp"
#include "ImFrame/Tree/Primitives/Box.hpp"
#include "ImFrame/Tree/Primitives/Expanded.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/GestureRegion.hpp"
#include "ImFrame/Tree/Primitives/Stack.hpp"
#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Widgets/Button.hpp"
#include "ImFrame/Widgets/Checkbox.hpp"
#include "ImFrame/Widgets/Radio.hpp"

#include <memory>
#include <optional>
#include <typeindex>
#include <vector>

namespace ImFrame::Tree {

/**
 * @class    WidgetTestDriver
 * @brief    Drives a `Component`'s Mount/Update/Layout/Paint cycle for unit testing
 *
 * @tparam   T  Any type satisfying `Tree::Component` (has `Build() const`).
 * @since    2.7.0
 *
 * @example
 * @code
 * struct Counter {
 *     struct S { int count = 0; };
 *     State<S> state;
 *
 *     [[nodiscard]] Widget Build() const {
 *         return ButtonWidget("+").OnClick([s = state] { s.Set([](S& st) { ++st.count; }); });
 *     }
 * };
 *
 * WidgetTestDriver driver{Counter{}};
 * driver.Build();
 * SimulateClick(*FindDescendant<Widgets::ButtonWidget>(driver.Root()));
 * driver.Build(); // re-render after the click
 * REQUIRE(driver.Component().state.Get().count == 1);
 * @endcode
 */
template <Component T>
class WidgetTestDriver {
public:
    explicit WidgetTestDriver(T component) : _component(std::move(component)) {}

    /**
     * @brief    Runs one `Build()` cycle and reconciles it against the previous one.
     *
     * Mounts on the first call; every subsequent call updates in place
     * (or remounts, if the built `Widget`'s concrete type changed). Call
     * again after mutating bound state (directly via `Component()`, or via a
     * fired delegate from `SimulateClick()`) to re-render.
     */
    void Build() {
        Widget built = _component.Build();
        if (_root && _root->CanUpdate(built)) {
            _root->Update(built);
        } else {
            if (_root) { _root->Unmount(); }
            _root = built.CreateElement();
            _root->Mount(nullptr, 0, built);
        }
        _lastBuilt = built;
    }

    /**
     * @brief    Runs `Layout()` + `Paint()` against the mounted tree.
     * @param[in] constraints  Available size; defaults to a generous 800x600 canvas.
     * @throws   Nothing; asserts (via null-deref) if called before `Build()`.
     */
    void LayoutAndPaint(BoxConstraints constraints = BoxConstraints::Loose({800.0f, 600.0f})) {
        (void)_root->Layout(constraints);
        _root->Paint({0.0f, 0.0f});
    }

    /// The most recent `Build()` output, for structural assertions via `Widget::As<SomeType>()`.
    [[nodiscard]] const Widget& Root() const noexcept { return *_lastBuilt; }

    /// The component under test — read/mutate fields directly (e.g. bound pointers, `State<T>` members).
    [[nodiscard]] T& Component() noexcept { return _component; }
    [[nodiscard]] const T& Component() const noexcept { return _component; }

private:
    T                         _component;
    std::unique_ptr<Element>  _root;
    std::optional<Widget>     _lastBuilt;
};

} // namespace ImFrame::Tree

/// @internal Normalises each known primitive's differently-shaped child accessor into a flat list.
/// Lives in the single project-wide `ImFrame::Internal::` (not a `Tree`-nested one) per the
/// Phase 27 `DECISIONS.md` lesson: unqualified `Internal::` lookup from other Tree headers
/// resolves to the nearest enclosing namespace of that name, and a second, module-nested
/// `Internal` would collide with it.
namespace ImFrame::Internal {

[[nodiscard]] inline std::vector<Tree::Widget> ChildrenOf(const Tree::Widget& w) {
    using Tree::Primitives::Box;
    using Tree::Primitives::Expanded;
    using Tree::Primitives::Flex;
    using Tree::Primitives::GestureRegion;
    using Tree::Primitives::Stack;

    if (w.TypeId() == typeid(Box)) {
        const auto& box = w.As<Box>();
        if (box.GetChild()) { return {*box.GetChild()}; }
        return {};
    }
    if (w.TypeId() == typeid(Flex))          { return w.As<Flex>().GetChildren(); }
    if (w.TypeId() == typeid(Stack))         { return w.As<Stack>().GetChildren(); }
    if (w.TypeId() == typeid(Expanded))      { return {w.As<Expanded>().GetChild()}; }
    if (w.TypeId() == typeid(Tree::Portal))  { return {w.As<Tree::Portal>().GetChild()}; }
    if (w.TypeId() == typeid(GestureRegion)) {
        const auto& gesture = w.As<GestureRegion>();
        if (gesture.GetChild()) { return {*gesture.GetChild()}; }
        return {};
    }
    // SizedBox, Spacer, Text have no children. VirtualList's rows are built
    // lazily and unbounded — deliberately not expanded here (see file comment).
    return {};
}

} // namespace ImFrame::Internal

namespace ImFrame::Tree {

/**
 * @brief    Depth-first search for the first descendant of type `Target` (or the root itself).
 *
 * Only recognises the built-in `Tree::Primitives` types plus a handful of
 * ImFrame's own interactive widgets that wrap a single/no child (`Box`,
 * `Flex`, `Stack`, `Expanded`, `Portal`, `GestureRegion`) — see the file
 * comment for what this deliberately does not cover.
 *
 * @tparam   Target  The concrete widget/primitive type to search for.
 * @param[in] root    Subtree to search, typically `driver.Root()`.
 * @return   The first matching value found, or `std::nullopt`.
 * @since    2.7.0
 */
template <typename Target>
[[nodiscard]] std::optional<Target> FindDescendant(const Widget& root) {
    if (root.TypeId() == typeid(Target)) {
        return root.template As<Target>();
    }
    for (const Widget& child : Internal::ChildrenOf(root)) {
        if (std::optional<Target> found = FindDescendant<Target>(child)) {
            return found;
        }
    }
    return std::nullopt;
}

/**
 * @brief    Fires the click/toggle behaviour of a widget, if it's a recognised interactive type.
 *
 * `ButtonWidget`/`Tree::Primitives::GestureRegion` fire their `OnClick`
 * delegate. `CheckboxWidget` toggles its bound `bool*` and fires `OnChange`.
 * `RadioWidget` sets its bound `int*` to its option (matching
 * `ImGui::RadioButton`'s own click semantics — it has no `OnChange`).
 *
 * @param[in] widget  A `Widget` obtained from `FindDescendant<T>()` (or any
 *                     `Widget` value known to wrap one of the recognised types).
 * @return   `true` if `widget` was a recognised clickable type (whether or
 *           not it actually had a delegate/pointer bound); `false` otherwise.
 * @since    2.7.0
 */
[[nodiscard]] inline bool SimulateClick(const Widget& widget) {
    if (widget.TypeId() == typeid(Widgets::ButtonWidget)) {
        const auto& button = widget.As<Widgets::ButtonWidget>();
        if (button.GetOnClick()) { button.GetOnClick()(); }
        return true;
    }
    if (widget.TypeId() == typeid(Widgets::CheckboxWidget)) {
        const auto& checkbox = widget.As<Widgets::CheckboxWidget>();
        if (bool* value = checkbox.GetValue()) {
            *value = !*value;
            if (checkbox.GetOnChange()) { checkbox.GetOnChange()(*value); }
        }
        return true;
    }
    if (widget.TypeId() == typeid(Widgets::RadioWidget)) {
        const auto& radio = widget.As<Widgets::RadioWidget>();
        if (int* value = radio.GetValue()) { *value = radio.GetOption(); }
        return true;
    }
    if (widget.TypeId() == typeid(Primitives::GestureRegion)) {
        const auto& gesture = widget.As<Primitives::GestureRegion>();
        if (gesture.GetOnClick()) { gesture.GetOnClick()(); }
        return true;
    }
    return false;
}

} // namespace ImFrame::Tree
