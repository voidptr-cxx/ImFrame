/**
 * @file     main.cpp
 * @brief    Full-featured ImFrame showcase demonstrating widgets, layouts, animations, overlays, and plots
 *
 * @internal
 * Demonstrates every major subsystem added in Phases 7–17:
 *   - Application / DockSpace / persistent layouts
 *   - Theme engine (Dracula, Nord, CatppuccinMocha, Light) with runtime switching
 *   - All core widgets (Button, Slider, TextInput, Checkbox, Combo, ProgressBar, etc.)
 *   - Layout containers (Panel, HStack, VStack, Grid, ScrollArea)
 *   - Animation (AnimatedValue<float>)
 *   - Overlays (Toast, Modal)
 *   - Virtualised Table (10 000 rows, per-column renderer mode)
 *   - Plots (LinePlot, BarPlot)
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  1.8.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/ImFrame.hpp"
#include "GLFWOpenGL3Backend.hpp"

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <string>
#include <vector>

// ─── Using declarations ───────────────────────────────────────────────────────

using namespace ImFrame;
using namespace ImFrame::Widgets;
using namespace ImFrame::Layout;
using namespace ImFrame::Overlay;
using namespace ImFrame::Anim;

// ─── Synthetic table data ─────────────────────────────────────────────────────

struct Row {
    int         id;
    std::string name;
    double      value;
};

static std::vector<Row> BuildTableRows() {
    std::vector<Row> rows;
    rows.reserve(10'000);
    for (int i = 0; i < 10'000; ++i)
        rows.push_back({i, "Item " + std::to_string(i), std::sin(i * 0.01) * 1000.0});
    return rows;
}

// ─── State ────────────────────────────────────────────────────────────────────

struct DemoState {
    // ─── Theme ────────────────────────────────────────────────────────────────
    int themeIndex = 0;

    // ─── Widget panel state ───────────────────────────────────────────────────
    bool        checkA        = true;
    bool        checkB        = false;
    float       sliderVal     = 0.4f;
    std::string inputText     = "Hello, ImFrame!";
    std::string comboSelected = "Option A";
    static inline const std::array<std::string, 4> comboOpts{
        "Option A", "Option B", "Option C", "Option D"
    };

    // ─── Modal ────────────────────────────────────────────────────────────────
    Modal confirmModal{"Confirm##confirm_demo"};

    // ─── Animation ────────────────────────────────────────────────────────────
    AnimatedValue<float> fadeBar{0.0f, 5.0f};
    bool                 fadeTarget  = false;
    float                tweenClock  = 0.0f;

    // ─── Table ────────────────────────────────────────────────────────────────
    Table            table{"##demo_table", 3};
    std::vector<Row> rows;
    bool             tableReady = false;

    // ─── Plots ────────────────────────────────────────────────────────────────
    std::array<double, 200> xs{};
    std::array<double, 200> sine{};
    std::array<double, 200> cosine{};
    std::array<double, 6>   barVals{};

    // ─── Init ─────────────────────────────────────────────────────────────────
    void Init() {
        rows = BuildTableRows();

        table.Scrollable().Striped().Borders().OuterSize(0.0f, 280.0f)
             .Column(ColumnDef("ID")
                 .Width(60.0f).WidthMode(ColumnWidthMode::Fixed).SortEnabled()
                 .Renderer([this](int r) { ImGui::Text("%d", rows[r].id); }))
             .Column(ColumnDef("Name")
                 .Width(200.0f).WidthMode(ColumnWidthMode::Fixed).SortEnabled()
                 .Renderer([this](int r) { ImGui::TextUnformatted(rows[r].name.c_str()); }))
             .Column(ColumnDef("Value")
                 .SortEnabled()
                 .Renderer([this](int r) { ImGui::Text("%.3f", rows[r].value); }));
        tableReady = true;

        for (int i = 0; i < 200; ++i) {
            xs[i]     = static_cast<double>(i) / 20.0;
            sine[i]   = std::sin(xs[i]);
            cosine[i] = std::cos(xs[i]);
        }
        for (int i = 0; i < 6; ++i)
            barVals[i] = static_cast<double>(i * i + 1);
    }
};

// ─── Panel renderers ─────────────────────────────────────────────────────────

static void DrawMenuBar(App::Application& app, DemoState& state) {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("Themes")) {
        static const std::array<const char*, 4> names{"Dracula", "Nord", "Catppuccin Mocha", "Light"};
        for (int i = 0; i < 4; ++i) {
            if (ImGui::MenuItem(names[i], nullptr, state.themeIndex == i)) {
                state.themeIndex = i;
                switch (i) {
                    case 0: app.WithTheme(Themes::Dracula);         break;
                    case 1: app.WithTheme(Themes::Nord);            break;
                    case 2: app.WithTheme(Themes::CatppuccinMocha); break;
                    case 3: app.WithTheme(Themes::Light);           break;
                }
                ToastInfo("Theme changed", names[i]);
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Layouts")) {
        if (ImGui::MenuItem("Save layout"))  app.GetDockSpace().SaveLayout("default");
        if (ImGui::MenuItem("Load layout"))  app.GetDockSpace().LoadLayout("default");
        if (ImGui::MenuItem("Reset layout")) app.GetDockSpace().ResetLayout();
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

static void DrawWidgetsPanel(DemoState& state) {
    ImGui::Begin("Widgets");

    ImGui::SeparatorText("Buttons");
    Button("Primary")
        .Width(100.0f)
        .OnClick([] { ToastSuccess("Clicked!", "Primary button pressed."); })
        .Show();
    ImGui::SameLine();
    Button("Disabled").Width(100.0f).Disabled().Show();

    ImGui::SeparatorText("Text Input");
    TextInput<std::string>("##input", state.inputText)
        .Hint("Type something…")
        .Width(260.0f)
        .Show();

    ImGui::SeparatorText("Slider");
    Slider<float>("Float##sl", state.sliderVal, 0.0f, 1.0f).Width(260.0f).Show();

    ImGui::SeparatorText("Checkboxes");
    Checkbox("Feature A", state.checkA).Show();
    ImGui::SameLine();
    Checkbox("Feature B", state.checkB).Show();

    ImGui::SeparatorText("Combo");
    Combo<std::string>("Pick one", state.comboSelected,
                       std::span<const std::string>{state.comboOpts})
        .Width(180.0f)
        .Show();

    ImGui::SeparatorText("Progress");
    ProgressBar(state.sliderVal).Size({-1.0f, 0.0f}).Show();

    ImGui::SeparatorText("Toasts");
    Button("Info").OnClick([]  { ToastInfo("Info",    "This is informational."); }).Show();
    ImGui::SameLine();
    Button("Success").OnClick([]{ ToastSuccess("Done","Operation succeeded.");   }).Show();
    ImGui::SameLine();
    Button("Warn").OnClick([]  { ToastWarning("Warn", "Something looks off.");  }).Show();
    ImGui::SameLine();
    Button("Error").OnClick([] { ToastError("Error",  "Something went wrong!"); }).Show();

    ImGui::SeparatorText("Modal");
    Button("Open Modal").Width(120.0f).OnClick([&state] { state.confirmModal.Open(); }).Show();

    if (auto scope = state.confirmModal.Begin()) {
        Text("Are you sure you want to proceed?").Wrapped().Show();
        ImGui::Spacing();
        Button("Yes").Width(80.0f).OnClick([&state] {
            state.confirmModal.Close();
            ToastSuccess("Confirmed!");
        }).Show();
        ImGui::SameLine();
        Button("No").Width(80.0f).OnClick([&state] {
            state.confirmModal.Close();
        }).Show();
    }

    ImGui::End();
}

static void DrawLayoutPanel(DemoState& /*state*/) {
    ImGui::Begin("Layouts");

    ImGui::SeparatorText("HStack (horizontal row)");
    HStack(8.0f).Render(
        Button("Left").Width(90.0f),
        Button("Center").Width(90.0f),
        Button("Right").Width(90.0f)
    );

    ImGui::SeparatorText("VStack (vertical column)");
    VStack(4.0f).Render(
        Button("Row 1").Width(160.0f),
        Button("Row 2").Width(160.0f),
        Button("Row 3").Width(160.0f)
    );

    ImGui::SeparatorText("Grid (2 columns)");
    Grid(2).Render(
        Button("Cell A").Width(110.0f),
        Button("Cell B").Width(110.0f),
        Button("Cell C").Width(110.0f),
        Button("Cell D").Width(110.0f)
    );

    ImGui::SeparatorText("Panel (coloured child window)");
    if (auto scope = Panel("##blue_panel")
            .Size({200.0f, 50.0f})
            .Border(true)
            .Background({0.2f, 0.4f, 0.8f, 1.0f})
            .Begin()) {
        Text("Inside a blue panel").Show();
    }

    ImGui::SeparatorText("ScrollArea");
    {
        ScrollArea scroll("##scroll_demo");
        scroll.Size({0.0f, 80.0f}).VerticalBar(true);
        if (auto scope = scroll.Begin()) {
            for (int i = 0; i < 20; ++i)
                Text("Scrollable row " + std::to_string(i)).Show();
        }
    }

    ImGui::End();
}

static void DrawAnimPanel(DemoState& state) {
    ImGui::Begin("Animation");

    ImGui::SeparatorText("AnimatedValue (exponential decay)");
    Text("Current value tracks toward target at speed 5.").Show();
    ProgressBar(state.fadeBar.Value()).Size({-1.0f, 0.0f}).Show();

    Button("Toggle target").Width(140.0f).OnClick([&state] {
        state.fadeTarget = !state.fadeTarget;
        state.fadeBar.SetTarget(state.fadeTarget ? 1.0f : 0.0f);
    }).Show();

    ImGui::SeparatorText("Sine oscillator (tweenClock)");
    float tweened = static_cast<float>(
        std::sin(state.tweenClock * std::numbers::pi_v<float>) * 0.5 + 0.5);
    ProgressBar(tweened).Size({-1.0f, 0.0f}).Show();

    ImGui::End();
}

static void DrawTablePanel(DemoState& state) {
    ImGui::Begin("Table (10 000 rows)");

    if (state.tableReady)
        state.table.Render(static_cast<int>(state.rows.size()));

    ImGui::End();
}

static void DrawPlotPanel(DemoState& state) {
    ImGui::Begin("Plots");

    ImGui::SeparatorText("Line plot — sin & cos");
    LinePlot("##lineplot")
        .Size(-1.0f, 200.0f)
        .XLabel("x")
        .YLabel("y")
        .Series("sin(x)", std::span{state.xs}, std::span{state.sine})
        .Series("cos(x)", std::span{state.xs}, std::span{state.cosine})
        .Show();

    ImGui::SeparatorText("Bar plot — y = x² + 1");
    BarPlot("##barplot")
        .Size(-1.0f, 150.0f)
        .Bars("y = x²+1", std::span{state.barVals})
        .Show();

    ImGui::End();
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main()
{
    DemoState state;
    state.Init();

    App::Application app(
        std::make_unique<Internal::GLFWOpenGL3Backend>(),
        WindowConfig{
            .Title     = "ImFrame Demo",
            .Width     = 1440,
            .Height    = 900,
            .VSync     = true,
            .Docking   = true,
            .Viewports = false,
        }
    );

    app.WithTheme(Themes::Dracula)
       .WithMenuBar(false)  // menu rendered via ImGui::BeginMainMenuBar() in OnUi
       .OnUpdate([&state](float dt) {
           state.fadeBar.Update(dt);
           state.tweenClock += dt * 0.4f;
       })
       .OnUi([&app, &state]() {
           DrawMenuBar(app, state);
           DrawWidgetsPanel(state);
           DrawLayoutPanel(state);
           DrawAnimPanel(state);
           DrawTablePanel(state);
           DrawPlotPanel(state);
       });

    if (auto result = app.Run(); !result)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
