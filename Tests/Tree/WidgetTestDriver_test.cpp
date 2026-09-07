/**
 * @file     WidgetTestDriver_test.cpp
 * @brief    Tests for `Tree::WidgetTestDriver`, `FindDescendant`, and `SimulateClick`
 *
 * Dogfoods the driver against ImFrame's own `ButtonWidget`/`CheckboxWidget`/
 * `Text` to prove it correctly reduces the Mount/Update boilerplate every
 * other `Tests/Tree/*.cpp` file hand-rolls (see `Reconciler_test.cpp`'s
 * manual `CreateElement()`/`Mount()`/`Update()` dance) while still driving
 * real `Tree::State<T>` dirty-tracking semantics.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-11
 * @version  2.7.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Tree/State.hpp"
#include "ImFrame/Tree/WidgetTestDriver.hpp"
#include "ImFrame/Widgets/Button.hpp"
#include "ImFrame/Widgets/Checkbox.hpp"
#include "ImFrame/Widgets/Radio.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Tree;
using namespace ImFrame::Tree::Primitives;
using namespace ImFrame::Widgets;

namespace {

/// A minimal stateful component — a button that increments its own counter.
struct Counter {
    struct S { int count = 0; };
    State<S> state;

    [[nodiscard]] Widget Build() const {
        return ButtonWidget("+").OnClick([s = state] { s.Set([](S& st) { ++st.count; }); });
    }
};

/// A component composing a checkbox and a text label reflecting its value.
/// Binds via caller-owned pointers (the established pattern throughout the
/// codebase — every *Widget with mutable state binds this way, since
/// Component::Build() is const and can't itself own mutable widget state).
struct LabelledCheckbox {
    bool* checked;
    int*  changeCount;

    [[nodiscard]] Widget Build() const {
        return Flex(Flex::Axis::Vertical).Children({
            CheckboxWidget("Enabled", checked)
                .OnChange([changeCount = changeCount](bool) { ++(*changeCount); }),
            Text(*checked ? "on" : "off"),
        });
    }
};

/// A component with no clickable widgets at all, for FindDescendant-miss coverage.
struct JustText {
    [[nodiscard]] Widget Build() const { return Text("hello"); }
};

} // namespace

TEST_CASE("WidgetTestDriver: Build() mounts and re-renders a stateful component", "[tree][widgettestdriver]") {
    WidgetTestDriver driver{Counter{}};
    driver.Build();

    REQUIRE(driver.Component().state.Get().count == 0);

    std::optional<ButtonWidget> button = FindDescendant<ButtonWidget>(driver.Root());
    REQUIRE(button.has_value());
    REQUIRE(SimulateClick(*button));

    // The click fired State<S>::Set() synchronously; a fresh Build() reflects it.
    driver.Build();
    REQUIRE(driver.Component().state.Get().count == 1);

    REQUIRE(SimulateClick(*FindDescendant<ButtonWidget>(driver.Root())));
    driver.Build();
    REQUIRE(driver.Component().state.Get().count == 2);
}

TEST_CASE("WidgetTestDriver: SimulateClick toggles CheckboxWidget and fires OnChange", "[tree][widgettestdriver]") {
    bool checked     = false;
    int  changeCount = 0;
    WidgetTestDriver driver{LabelledCheckbox{&checked, &changeCount}};
    driver.Build();

    std::optional<Text> label = FindDescendant<Text>(driver.Root());
    REQUIRE(label.has_value());
    REQUIRE(label->GetContent() == "off");

    std::optional<CheckboxWidget> checkbox = FindDescendant<CheckboxWidget>(driver.Root());
    REQUIRE(checkbox.has_value());
    REQUIRE(SimulateClick(*checkbox));

    REQUIRE(checked);
    REQUIRE(changeCount == 1);

    driver.Build();
    label = FindDescendant<Text>(driver.Root());
    REQUIRE(label->GetContent() == "on");
}

TEST_CASE("WidgetTestDriver: SimulateClick sets a RadioWidget's bound value to its option", "[tree][widgettestdriver]") {
    int mode = 0;
    RadioWidget radio("Nearest", &mode, 1);
    Widget widget = radio;

    REQUIRE(SimulateClick(widget));
    REQUIRE(mode == 1);
}

TEST_CASE("WidgetTestDriver: FindDescendant returns nullopt when the type isn't present", "[tree][widgettestdriver]") {
    WidgetTestDriver driver{JustText{}};
    driver.Build();

    REQUIRE_FALSE(FindDescendant<ButtonWidget>(driver.Root()).has_value());
    REQUIRE(FindDescendant<Text>(driver.Root()).has_value());
}

TEST_CASE("WidgetTestDriver: SimulateClick returns false for a non-clickable widget", "[tree][widgettestdriver]") {
    Widget text = Text("static");
    REQUIRE_FALSE(SimulateClick(text));
}
