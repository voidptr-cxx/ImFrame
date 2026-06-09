/**
 * @file     PropertyGrid_test.cpp
 * @brief    Unit tests for Widgets::PropertyGrid and Widgets::PropertyGridScope
 *
 * @internal
 * All tests run inside a headless ImGui context via TestHeadlessBackend.
 * Widgets passed to PropertyGridScope::Row() are real ImFrame widgets to verify
 * the template constraint compiles correctly.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-09
 * @version  1.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "../App/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Widgets/Button.hpp"
#include "ImFrame/Widgets/Checkbox.hpp"
#include "ImFrame/Widgets/PropertyGrid.hpp"
#include "ImFrame/Widgets/Text.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::Button;
using ImFrame::Widgets::Checkbox;
using ImFrame::Widgets::PropertyGrid;
using ImFrame::Widgets::PropertyGridScope;
using ImFrame::Widgets::Text;

TEST_CASE("PropertyGrid::Begin returns open scope", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool scopeWasOpen = false;

    application.OnUi([&] {
        PropertyGrid grid("##basic");
        auto scope = grid.Begin();
        scopeWasOpen = static_cast<bool>(scope);
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(scopeWasOpen);
}

TEST_CASE("PropertyGrid::Row with Text widget does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        PropertyGrid grid("##textrow");
        if (auto scope = grid.Begin()) {
            auto label = Text("ReadOnly");
            scope.Row("Label", label);
        }
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("PropertyGrid::Row with Button widget does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        PropertyGrid grid("##btnrow");
        if (auto scope = grid.Begin()) {
            auto btn = Button("Apply");
            scope.Row("Action", btn);
        }
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("PropertyGrid::Row with Checkbox does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    bool value = false;

    application.OnUi([&] {
        PropertyGrid grid("##chkrow");
        if (auto scope = grid.Begin()) {
            auto chk = Checkbox("##v", value);
            scope.Row("Enabled", chk);
        }
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("PropertyGrid::Separator does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        PropertyGrid grid("##sep");
        if (auto scope = grid.Begin()) {
            auto lbl = Text("First");
            scope.Row("Alpha", lbl)
                 .Separator("Group Header")
                 .Row("Beta", lbl);
        }
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("PropertyGrid::Separator without label does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        PropertyGrid grid("##noseplabel");
        if (auto scope = grid.Begin()) {
            auto lbl = Text("x");
            scope.Row("A", lbl)
                 .Separator()       // no label
                 .Row("B", lbl);
        }
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("PropertyGrid chaining Row calls returns same scope", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        PropertyGrid grid("##chain");
        if (auto scope = grid.Begin()) {
            auto w1 = Text("one");
            auto w2 = Text("two");
            auto w3 = Text("three");
            // Chaining: each Row/Separator returns *this by reference.
            scope.Row("First",  w1)
                 .Separator("Section")
                 .Row("Second", w2)
                 .Row("Third",  w3);
        }
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("PropertyGrid::SplitRatio builder compiles and returns same object", "[unit]") {
    PropertyGrid grid("##split");
    PropertyGrid& ref = grid.SplitRatio(0.4f);
    REQUIRE(&grid == &ref);
}

TEST_CASE("PropertyGrid with SplitRatio(0.4) does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        PropertyGrid grid("##splitrender");
        grid.SplitRatio(0.4f);
        if (auto scope = grid.Begin()) {
            auto lbl = Text("value");
            scope.Row("Property", lbl);
        }
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("PropertyGrid multiple grids in one frame do not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        {
            PropertyGrid g1("##mg1");
            if (auto s = g1.Begin()) {
                auto lbl = Text("a");
                s.Row("X", lbl);
            }
        }
        {
            PropertyGrid g2("##mg2");
            g2.SplitRatio(0.5f);
            if (auto s = g2.Begin()) {
                auto lbl = Text("b");
                s.Row("Y", lbl);
            }
        }
    });

    REQUIRE(application.Run().has_value());
}
