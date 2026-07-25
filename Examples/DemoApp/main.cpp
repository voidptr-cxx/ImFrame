/**
 * @file     main.cpp
 * @brief    Full-featured ImFrame showcase demonstrating widgets, layouts, animations, overlays, and plots
 *
 * @internal
 * Demonstrates every major subsystem through Phase 30:
 *   - Application / DockSpace / persistent layouts
 *   - Theme engine (Dracula, Nord, CatppuccinMocha, Light) with runtime switching
 *   - Widget-tree Components (ButtonWidget, SliderWidget, TextInputWidget, CheckboxWidget,
 *     ComboWidget, ProgressBarWidget, ModalWidget, etc.), driven per-panel via the local
 *     `TreePanel` helper below rather than `Application::SetRoot()` — `SetRoot()` always
 *     paints into a single fixed, non-dockable root window (see `DECISIONS.md`, Phase 27:
 *     "real docking integration is deferred"), but this demo's six panels are each
 *     independently dockable `ImGui::Begin()` windows. `TreePanel` drives one small
 *     `Tree::Element`'s Mount/Update/Layout/Paint cycle using only public `Tree::Widget`/
 *     `Tree::Element` API, inside whichever window already opened.
 *   - Layout primitives (Box, Flex, GridWidget, VirtualList)
 *   - Animation (AnimatedValue<float>)
 *   - Overlays (Toast, ModalWidget)
 *   - Virtualised Table (10 000 rows, TableWidget)
 *   - Plots (LinePlot, BarPlot)
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  2.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/ImFrame.hpp"
#include "ImFrame/Icons/Icons.hpp"
#include "GLFWOpenGL3Backend.hpp"
#include "Rendering/Renderers/ImGuiCompatRenderer.hpp"

#include <imgui.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

// ─── Using declarations ───────────────────────────────────────────────────────

using namespace ImFrame;
using namespace ImFrame::Widgets;
using namespace ImFrame::Layout;
using namespace ImFrame::Overlay;
using namespace ImFrame::Anim;
using namespace ImFrame::Icons;
using namespace ImFrame::Tree;
using namespace ImFrame::Tree::Primitives;

// ─── TreePanel ────────────────────────────────────────────────────────────────

/**
 * @brief  Drives one widget-tree Component's Mount/Update + Layout/Paint cycle
 *         inside the caller's own already-open ImGui window.
 *
 * `Application::SetRoot()` is not used here because it always paints into a
 * single fixed, non-dockable root window — this demo needs each panel to stay
 * an independently dockable `ImGui::Begin()` window. `Tree::Widget::
 * CreateElement()` and `Tree::Element::Mount/Update/CanUpdate/Layout/Paint`
 * are public and need no ImGui window of their own (`Paint()` only calls
 * `SetCursorScreenPos` and draws), so one `TreePanel` per panel reproduces
 * exactly what `Internal::Reconciler::Show()` does, minus the window it opens.
 */
class TreePanel {
public:
    template <Component T>
    void Show(const T& component) {
        Widget widget = component.Build();
        if (_root && _root->CanUpdate(widget)) {
            _root->Update(widget);
        } else {
            if (_root) { _root->Unmount(); }
            _root = widget.CreateElement();
            _root->Mount(nullptr, 0, widget);
        }
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        (void)_root->Layout(BoxConstraints::Loose({avail.x, avail.y}));
        const ImVec2 cursor = ImGui::GetCursorScreenPos();

        _commandBuffer.Reset();
        _root->Paint(_commandBuffer, {cursor.x, cursor.y});
        _renderer.Render(_commandBuffer);
    }

private:
    std::unique_ptr<Element>            _root;
    Rendering::CommandBuffer            _commandBuffer;
    Internal::ImGuiCompatRenderer       _renderer;
};

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
    bool showConfirmModal = false;

    // ─── Animation ────────────────────────────────────────────────────────────
    AnimatedValue<float> fadeBar{0.0f, 5.0f};
    bool                 fadeTarget  = false;
    float                tweenClock  = 0.0f;

    // ─── Table ────────────────────────────────────────────────────────────────
    std::vector<Row> rows;

    // ─── Plots ────────────────────────────────────────────────────────────────
    std::array<double, 200> xs{};
    std::array<double, 200> sine{};
    std::array<double, 200> cosine{};
    std::array<double, 6>   barVals{};

    // ─── Widget-tree panels ───────────────────────────────────────────────────
    // Held here (rather than as separate locals in main()) so `OnUi`'s lambda
    // only needs to capture `[&app, &state]` — two pointers, exactly the
    // 16-byte `Utility::Delegate<void()>` small-buffer limit. Capturing each
    // TreePanel individually would overflow it.
    TreePanel widgetsPanel;
    TreePanel layoutsPanel;
    TreePanel animPanel;
    TreePanel tablePanel;
    TreePanel iconButtonsPanel;

    // ─── Init ─────────────────────────────────────────────────────────────────
    void Init() {
        rows = BuildTableRows();

        for (int i = 0; i < 200; ++i) {
            xs[i]     = static_cast<double>(i) / 20.0;
            sine[i]   = std::sin(xs[i]);
            cosine[i] = std::cos(xs[i]);
        }
        for (int i = 0; i < 6; ++i)
            barVals[i] = static_cast<double>(i * i + 1);
    }
};

// ─── Widget-tree panel Components ──────────────────────────────────────────────

/// Buttons demonstrating `ButtonWidget::Icon()` — used inside the raw-ImGui Icons panel.
struct IconButtonsRoot {
    [[nodiscard]] Widget Build() const {
        return Flex(Flex::Axis::Horizontal).Gap(8.0f).Children({
            ButtonWidget("Save").Icon(Fa::FloppyDisk).Width(110.0f),
            ButtonWidget("Download").Icon(Fa::Download).Width(110.0f),
            ButtonWidget("Search").Icon(Fa::MagnifyingGlass).Width(110.0f),
            ButtonWidget("Settings").Icon(Fa::Gear).Width(110.0f),
        });
    }
};

struct WidgetsPanelRoot {
    DemoState* state;

    [[nodiscard]] Widget Build() const {
        return Flex(Flex::Axis::Vertical).Gap(8.0f).Children({
            SeparatorWidget().Label("Buttons"),
            Flex(Flex::Axis::Horizontal).Gap(8.0f).Children({
                ButtonWidget("Primary").Width(100.0f)
                    .OnClick([] { ToastSuccess("Clicked!", "Primary button pressed."); }),
                ButtonWidget("Disabled").Width(100.0f).Disabled(),
            }),

            SeparatorWidget().Label("Text Input"),
            TextInputWidget<std::string>("##input", &state->inputText)
                .Hint("Type something...")
                .Width(260.0f),

            SeparatorWidget().Label("Slider"),
            SliderWidget<float>("Float##sl", &state->sliderVal, 0.0f, 1.0f).Width(260.0f),

            SeparatorWidget().Label("Checkboxes"),
            Flex(Flex::Axis::Horizontal).Gap(8.0f).Children({
                CheckboxWidget("Feature A", &state->checkA),
                CheckboxWidget("Feature B", &state->checkB),
            }),

            SeparatorWidget().Label("Combo"),
            ComboWidget<std::string>("Pick one", &state->comboSelected,
                       std::span<const std::string>{DemoState::comboOpts})
                .Width(180.0f),

            SeparatorWidget().Label("Progress"),
            ProgressBarWidget(state->sliderVal).Size({-1.0f, 0.0f}),

            SeparatorWidget().Label("Toasts"),
            Flex(Flex::Axis::Horizontal).Gap(8.0f).Children({
                ButtonWidget("Info").Icon(Fa::CircleInfo)
                    .OnClick([] { ToastInfo("Info", "This is informational."); }),
                ButtonWidget("Success").Icon(Fa::CircleCheck)
                    .OnClick([] { ToastSuccess("Done", "Operation succeeded."); }),
                ButtonWidget("Warn").Icon(Fa::CircleExclamation)
                    .OnClick([] { ToastWarning("Warn", "Something looks off."); }),
                ButtonWidget("Error").Icon(Fa::CircleXmark)
                    .OnClick([] { ToastError("Error", "Something went wrong!"); }),
            }),

            SeparatorWidget().Label("Modal"),
            ButtonWidget("Open Modal").Icon(Fa::CircleExclamation).Width(140.0f)
                .OnClick([state = state] { state->showConfirmModal = true; }),
            ModalWidget("Confirm##confirm_demo", &state->showConfirmModal)
                .Content(Flex(Flex::Axis::Vertical).Gap(8.0f).Children({
                    Text("Are you sure you want to proceed?").Wrap(true),
                    Flex(Flex::Axis::Horizontal).Gap(8.0f).Children({
                        ButtonWidget("Yes").Width(80.0f).OnClick([state = state] {
                            state->showConfirmModal = false;
                            ToastSuccess("Confirmed!");
                        }),
                        ButtonWidget("No").Width(80.0f).OnClick([state = state] {
                            state->showConfirmModal = false;
                        }),
                    }),
                })),
        });
    }
};

struct LayoutsPanelRoot {
    [[nodiscard]] Widget Build() const {
        return Flex(Flex::Axis::Vertical).Gap(8.0f).Children({
            SeparatorWidget().Label("Flex, horizontal (was HStack)"),
            Flex(Flex::Axis::Horizontal).Gap(8.0f).Children({
                ButtonWidget("Left").Width(90.0f),
                ButtonWidget("Center").Width(90.0f),
                ButtonWidget("Right").Width(90.0f),
            }),

            SeparatorWidget().Label("Flex, vertical (was VStack)"),
            Flex(Flex::Axis::Vertical).Gap(4.0f).Children({
                ButtonWidget("Row 1").Width(160.0f),
                ButtonWidget("Row 2").Width(160.0f),
                ButtonWidget("Row 3").Width(160.0f),
            }),

            SeparatorWidget().Label("GridWidget (2 columns)"),
            GridWidget(2).Spacing(4.0f).Children({
                ButtonWidget("Cell A").Width(110.0f),
                ButtonWidget("Cell B").Width(110.0f),
                ButtonWidget("Cell C").Width(110.0f),
                ButtonWidget("Cell D").Width(110.0f),
            }),

            SeparatorWidget().Label("Box (coloured container, was Panel)"),
            Box().Width(200.0f).Height(50.0f)
                .BorderColor({1.0f, 1.0f, 1.0f, 1.0f}).BorderWidth(1.0f)
                .Background({0.2f, 0.4f, 0.8f, 1.0f})
                .Child(Text("Inside a coloured box")),

            SeparatorWidget().Label("VirtualList (scrollable, was ScrollArea)"),
            Box().Height(80.0f).Child(
                VirtualList(20, 20.0f, [](int i) -> Widget {
                    return Text("Scrollable row " + std::to_string(i));
                })
            ),
        });
    }
};

struct AnimationPanelRoot {
    DemoState* state;

    [[nodiscard]] Widget Build() const {
        const float tweened = static_cast<float>(
            std::sin(state->tweenClock * std::numbers::pi_v<float>) * 0.5 + 0.5);

        return Flex(Flex::Axis::Vertical).Gap(8.0f).Children({
            SeparatorWidget().Label("AnimatedValue (exponential decay)"),
            Text("Current value tracks toward target at speed 5."),
            ProgressBarWidget(state->fadeBar.Value()).Size({-1.0f, 0.0f}),
            ButtonWidget("Toggle target").Width(140.0f).OnClick([state = state] {
                state->fadeTarget = !state->fadeTarget;
                state->fadeBar.SetTarget(state->fadeTarget ? 1.0f : 0.0f);
            }),

            SeparatorWidget().Label("Sine oscillator (tweenClock)"),
            ProgressBarWidget(tweened).Size({-1.0f, 0.0f}),
        });
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

static void DrawIconsPanel(DemoState& state) {
    ImGui::Begin("Icons & Fonts");

    ImGui::SeparatorText("FontAwesome 6 Free — glyph reference");
    ImGui::TextWrapped("The FA6 icon font is merged into the default typeface via "
                       "Application::WithFont({ .isIconFont = true }). "
                       "Any FA6 glyph constant from ImFrame::Icons::Fa can be passed "
                       "directly to ImGui::Text or ButtonWidget::Icon.");

    ImGui::Spacing();

    struct IconEntry { const char* glyph; const char* name; };
    static constexpr std::array<IconEntry, 24> kIcons{{
        { Fa::House,             "House"          },
        { Fa::Gear,              "Gear"           },
        { Fa::MagnifyingGlass,   "Search"         },
        { Fa::Bell,              "Bell"           },
        { Fa::CircleCheck,       "CircleCheck"    },
        { Fa::CircleXmark,       "CircleXmark"    },
        { Fa::CircleExclamation, "Alert"          },
        { Fa::CircleInfo,        "Info"           },
        { Fa::Heart,             "Heart"          },
        { Fa::Bookmark,          "Bookmark"       },
        { Fa::Eye,               "Eye"            },
        { Fa::Copy,              "Copy"           },
        { Fa::FloppyDisk,        "Save"           },
        { Fa::Download,          "Download"       },
        { Fa::File,              "File"           },
        { Fa::Flag,              "Flag"           },
        { Fa::Globe,             "Globe"          },
        { Fa::Key,               "Key"            },
        { Fa::Lock,              "Lock"           },
        { Fa::Laptop,            "Laptop"         },
        { Fa::Link,              "Link"           },
        { Fa::Microphone,        "Microphone"     },
        { Fa::Moon,              "Moon"           },
        { Fa::Music,             "Music"          },
    }};

    if (ImGui::BeginTable("##icon_grid", 4, ImGuiTableFlags_SizingFixedFit)) {
        for (const auto& e : kIcons) {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(e.glyph);
            ImGui::SameLine(24.0f);
            ImGui::TextUnformatted(e.name);
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("ButtonWidget::Icon() fluent API");
    state.iconButtonsPanel.Show(IconButtonsRoot{});

    ImGui::SeparatorText("Inline icon + text (ImGui::Text)");
    ImGui::Text("%s  Home panel", Fa::House);
    ImGui::Text("%s  Locked resource", Fa::Lock);
    ImGui::Text("%s  New notification", Fa::Bell);
    ImGui::Text("%s  Global scope", Fa::Globe);

    ImGui::End();
}

static void DrawWidgetsPanel(DemoState& state) {
    ImGui::Begin("Widgets");
    state.widgetsPanel.Show(WidgetsPanelRoot{&state});
    ImGui::End();
}

static void DrawLayoutPanel(DemoState& state) {
    ImGui::Begin("Layouts");
    state.layoutsPanel.Show(LayoutsPanelRoot{});
    ImGui::End();
}

static void DrawAnimPanel(DemoState& state) {
    ImGui::Begin("Animation");
    state.animPanel.Show(AnimationPanelRoot{&state});
    ImGui::End();
}

static void DrawTablePanel(DemoState& state) {
    ImGui::Begin("Table (10 000 rows)");
    state.tablePanel.Show(
        TableWidget(static_cast<int>(state.rows.size()), 24.0f)
            .Column("ID",    [&state](int r) { return std::to_string(state.rows[r].id); })
            .Column("Name",  [&state](int r) { return state.rows[r].name; })
            .Column("Value", [&state](int r) { return std::format("{:.3f}", state.rows[r].value); })
    );
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
            .VSync     = VSyncMode::On,
            .Docking   = true,
            .Viewports = false,
        }
    );

    app.WithFont({ .path = "C:/Windows/Fonts/segoeui.ttf", .size = 16.0f })
       .WithFont({ .path = "Assets/Fonts/fa-solid-900.ttf", .size = 14.0f, .isIconFont = true })
       .WithTheme(Themes::Dracula)
       .WithMenuBar(false)  // menu rendered via ImGui::BeginMainMenuBar() in OnUi
       .OnUpdate([&state](float dt) {
           state.fadeBar.Update(dt);
           state.tweenClock += dt * 0.4f;
       })
       .OnUi([&app, &state]() {
           DrawMenuBar(app, state);
           DrawWidgetsPanel(state);
           DrawLayoutPanel(state);
           DrawIconsPanel(state);
           DrawAnimPanel(state);
           DrawTablePanel(state);
           DrawPlotPanel(state);
       });

    if (auto result = app.Run(); !result)
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
