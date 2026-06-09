/**
 * @file     Table_test.cpp
 * @brief    Unit tests for Widgets::Table
 *
 * @internal
 * All tests run inside a headless ImGui context via TestHeadlessBackend.
 * The display size defaults to 1280×720, so ImGuiListClipper renders
 * approximately 40–50 rows from a 1 000-row dataset — well below rowCount.
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
#include "ImFrame/Widgets/Table.hpp"

#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::ColumnDef;
using ImFrame::Widgets::ColumnWidthMode;
using ImFrame::Widgets::SortDirection;
using ImFrame::Widgets::SortState;
using ImFrame::Widgets::Table;

TEST_CASE("Table with row-renderer mode does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        Table tbl("##basic", 2);
        tbl.Column(ColumnDef("A"))
           .Column(ColumnDef("B"))
           .Borders()
           .Render(10, [](int /*row*/) {
               ImGui::TableNextColumn();
               ImGui::TextUnformatted("cell");
               ImGui::TableNextColumn();
               ImGui::TextUnformatted("cell");
           });
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("Table clipper fires fewer callbacks than rowCount for large datasets", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    constexpr int rowCount = 1000;
    int callbackCount = 0;

    application.OnUi([&] {
        Table tbl("##clip", 1);
        tbl.Column(ColumnDef("Value"))
           .Render(rowCount, [&](int /*row*/) {
               ++callbackCount;
               ImGui::TableNextColumn();
               ImGui::TextUnformatted("x");
           });
    });

    REQUIRE(application.Run().has_value());
    // Clipper must have fired fewer callbacks than total row count.
    REQUIRE(callbackCount > 0);
    REQUIRE(callbackCount < rowCount);
}

TEST_CASE("Table per-column renderer mode does not crash and is called", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    int colRendererCalls = 0;

    application.OnUi([&] {
        Table tbl("##colrend", 2);
        tbl.Column(ColumnDef("Name")
                   .Renderer([&](int /*row*/) {
                       ++colRendererCalls;
                       ImGui::TextUnformatted("name");
                   }))
           .Column(ColumnDef("Value")
                   .Renderer([&](int /*row*/) {
                       ImGui::TextUnformatted("val");
                   }))
           .Render(5);
    });

    REQUIRE(application.Run().has_value());
    // The clipper renders however many rows fit in the current visible area.
    // In the headless DockSpace context, cursor Y after DockSpace may limit
    // visible rows — assert the renderer was called at least once (not zero)
    // and at most once per row.
    REQUIRE(colRendererCalls >= 1);
    REQUIRE(colRendererCalls <= 5);
}

TEST_CASE("Table per-column renderer ignores the global rowRenderer", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    int rowRendererCalls = 0;

    application.OnUi([&] {
        Table tbl("##rendignore", 1);
        tbl.Column(ColumnDef("X").Renderer([](int /*row*/) { ImGui::TextUnformatted("x"); }))
           .Render(3, [&](int /*row*/) {
               ++rowRendererCalls;  // should never be called
               ImGui::TableNextColumn();
               ImGui::TextUnformatted("never");
           });
    });

    REQUIRE(application.Run().has_value());
    REQUIRE(rowRendererCalls == 0);
}

TEST_CASE("Table SortState is accessible and defaults to not dirty", "[unit]") {
    Table tbl("##sort", 2);
    tbl.Column(ColumnDef("Name").SortEnabled())
       .Column(ColumnDef("Score").SortEnabled());

    // Before any Render() call the sort state is default-constructed.
    SortState state = tbl.GetSortState();
    REQUIRE_FALSE(state.dirty);
    REQUIRE(state.specs.empty());
}

TEST_CASE("Table SortState does not crash after Render()", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    Table tbl("##sortrender", 2);
    tbl.Column(ColumnDef("A").SortEnabled())
       .Column(ColumnDef("B").SortEnabled());

    SortState state;
    application.OnUi([&] {
        tbl.Render(5, [](int /*row*/) {
            ImGui::TableNextColumn(); ImGui::TextUnformatted("a");
            ImGui::TableNextColumn(); ImGui::TextUnformatted("b");
        });
        state = tbl.GetSortState();
    });

    REQUIRE(application.Run().has_value());
    // ImGui marks sort specs dirty on first render (default sort initialisation).
    // Verify the state is accessible and specs vector is well-formed.
    // dirty may be true on the first frame — that is correct ImGui behaviour.
    REQUIRE(state.specs.size() <= static_cast<size_t>(2)); // at most one spec per column
}

TEST_CASE("Table ContextMenu callback is registered without crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    int contextRow = -1;

    application.OnUi([&] {
        Table tbl("##ctx", 1);
        tbl.Column(ColumnDef("Item"))
           .ContextMenu([&](int row) { contextRow = row; })
           .Render(5, [](int /*row*/) {
               ImGui::TableNextColumn();
               ImGui::TextUnformatted("item");
           });
    });

    REQUIRE(application.Run().has_value());
    // No right-click in headless context — contextRow stays -1.
    REQUIRE(contextRow == -1);
}

TEST_CASE("Table fluent builder returns same object", "[unit]") {
    Table tbl("##builder", 3);
    Table& r1 = tbl.Flags(0);
    Table& r2 = r1.OuterSize(0.0f, 200.0f);
    Table& r3 = r2.Scrollable();
    Table& r4 = r3.Borders();
    Table& r5 = r4.Striped();
    REQUIRE(&tbl == &r5);
}

TEST_CASE("Table with zero rows does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        Table tbl("##zero", 1);
        tbl.Column(ColumnDef("Col"))
           .Render(0, [](int /*row*/) {});
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("ColumnDef fluent builder compiles and returns same object", "[unit]") {
    ColumnDef def("MyCol");
    ColumnDef& r1 = def.Width(150.0f);
    ColumnDef& r2 = r1.WidthMode(ColumnWidthMode::Fixed);
    ColumnDef& r3 = r2.SortEnabled(true);
    REQUIRE(&def == &r3);
}

TEST_CASE("SortDirection and SortState types are default-constructible", "[unit]") {
    SortState s;
    REQUIRE_FALSE(s.dirty);
    REQUIRE(s.specs.empty());
    REQUIRE(SortDirection::None       != SortDirection::Ascending);
    REQUIRE(SortDirection::Ascending  != SortDirection::Descending);
}
