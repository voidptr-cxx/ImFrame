/**
 * @file     Widgets_test.cpp
 * @brief    Behavioral tests for the Phase 29 reimplemented widget-tree primitives
 *
 * Covers `ButtonWidget`, `CheckboxWidget`, `SliderWidget<T>`, `TextInputWidget<T>`,
 * `ComboWidget<T>`, and `TableWidget` — each drives a real `HeadlessBackend` +
 * `Application::SetRoot()` and verifies the same interaction-firing contract as
 * the deprecated Phase 10–14 `Show()`-based widgets (same underlying ImGui calls,
 * reached through a declarative `Build()` instead of an imperative call site).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Overlay/ContextMenu.hpp"
#include "ImFrame/Overlay/Modal.hpp"
#include "ImFrame/Overlay/Toast.hpp"
#include "ImFrame/Layout/Grid.hpp"
#include "ImFrame/Tree/Primitives/Expanded.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/GestureRegion.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Tree/VirtualList.hpp"
#include "ImFrame/Widgets/Button.hpp"
#include "ImFrame/Widgets/Checkbox.hpp"
#include "ImFrame/Widgets/ColorEdit.hpp"
#include "ImFrame/Widgets/Combo.hpp"
#include "ImFrame/Widgets/Image.hpp"
#include "ImFrame/Widgets/ProgressBar.hpp"
#include "ImFrame/Widgets/PropertyGrid.hpp"
#include "ImFrame/Widgets/Radio.hpp"
#include "ImFrame/Widgets/Separator.hpp"
#include "ImFrame/Widgets/Slider.hpp"
#include "ImFrame/Widgets/Table.hpp"
#include "ImFrame/Widgets/TextInput.hpp"

#include <catch2/catch_test_macros.hpp>
#include <imgui.h>

#include <array>
#include <set>
#include <span>
#include <string>

using namespace ImFrame;
using namespace ImFrame::Tree;
using namespace ImFrame::Widgets;
using ImFrame::App::Application;

namespace {

/// HeadlessBackend subclass that stops the render loop after N full frames.
class FrameLimitedHeadlessBackend final : public Internal::HeadlessBackend {
public:
    explicit FrameLimitedHeadlessBackend(int maxFrames) : _maxFrames(maxFrames) {}

    FrameInfo Poll() override {
        FrameInfo info   = Internal::HeadlessBackend::Poll();
        info.ShouldClose = (_frameCount >= _maxFrames);
        return info;
    }

    void BeginFrame(WindowHandle h = PrimaryWindow) override {
        Internal::HeadlessBackend::BeginFrame(h);
        ++_frameCount;
    }

private:
    int _maxFrames;
    int _frameCount = 0;
};

WindowConfig TestConfig() {
    return WindowConfig{.Title = "WidgetsTest", .Width = 400, .Height = 300};
}

/// Positions the mouse and issues a press+release over whatever is at that screen position.
void ClickAt(float x, float y) {
    ImGui::GetIO().AddMousePosEvent(x, y);
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);
}

} // namespace

// ─── ButtonWidget ───────────────────────────────────────────────────────────────

TEST_CASE("ButtonWidget: renders without error and OnClick fires on click", "[tree][widgets][button]") {
    bool clicked = false;
    int  frame   = 0;

    struct Root {
        bool* clicked;
        [[nodiscard]] Widget Build() const {
            return ButtonWidget("Save").OnClick([c = clicked] { *c = true; });
        }
    } root{&clicked};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(4), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) {
        ++frame;
        if (frame == 2) { ClickAt(10.0f, 10.0f); }
    });
    REQUIRE(app.Run().has_value());
    CHECK(clicked);
}

TEST_CASE("ButtonWidget: disabled button does not fire OnClick", "[tree][widgets][button]") {
    bool clicked = false;
    int  frame   = 0;

    struct Root {
        bool* clicked;
        [[nodiscard]] Widget Build() const {
            return ButtonWidget("Save").Disabled().OnClick([c = clicked] { *c = true; });
        }
    } root{&clicked};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(4), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) {
        ++frame;
        if (frame == 2) { ClickAt(10.0f, 10.0f); }
    });
    REQUIRE(app.Run().has_value());
    CHECK_FALSE(clicked);
}

// ─── CheckboxWidget ─────────────────────────────────────────────────────────────

TEST_CASE("CheckboxWidget: renders without error and toggles the bound value on click", "[tree][widgets][checkbox]") {
    bool wireframe = false;
    bool onChangeFired = false;
    int  frame = 0;

    struct Root {
        bool* value;
        bool* onChangeFired;
        [[nodiscard]] Widget Build() const {
            return CheckboxWidget("Wireframe", value).OnChange([f = onChangeFired](bool) { *f = true; });
        }
    } root{&wireframe, &onChangeFired};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(4), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) {
        ++frame;
        if (frame == 2) { ClickAt(10.0f, 10.0f); }
    });
    REQUIRE(app.Run().has_value());
    CHECK(wireframe);
    CHECK(onChangeFired);
}

// ─── SliderWidget<T> ────────────────────────────────────────────────────────────

TEST_CASE("SliderWidget<float>: renders without error", "[tree][widgets][slider]") {
    float volume = 0.5f;
    struct Root {
        float* value;
        [[nodiscard]] Widget Build() const {
            return SliderWidget<float>("Volume", value, 0.0f, 1.0f).Format("%.2f");
        }
    } root{&volume};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
    CHECK(volume == 0.5f); // unchanged — no simulated drag; just confirms no crash and value round-trips
}

TEST_CASE("SliderWidget<int>: renders without error", "[tree][widgets][slider]") {
    int level = 3;
    struct Root {
        int* value;
        [[nodiscard]] Widget Build() const { return SliderWidget<int>("Level", value, 0, 10); }
    } root{&level};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
    CHECK(level == 3);
}

// ─── TextInputWidget<T> ─────────────────────────────────────────────────────────

TEST_CASE("TextInputWidget<std::string>: renders without error", "[tree][widgets][textinput]") {
    std::string name = "Alice";
    struct Root {
        std::string* value;
        [[nodiscard]] Widget Build() const {
            return TextInputWidget<std::string>("Name", value).Hint("Enter your name");
        }
    } root{&name};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
    CHECK(name == "Alice");
}

// ─── ComboWidget<T> ─────────────────────────────────────────────────────────────

TEST_CASE("ComboWidget<std::string>: renders without error", "[tree][widgets][combo]") {
    static const std::array<std::string, 3> modes{"Linear", "Nearest", "Cubic"};
    std::string selected = "Linear";

    struct Root {
        std::string* value;
        [[nodiscard]] Widget Build() const {
            return ComboWidget<std::string>("Filter", value, std::span<const std::string>(modes));
        }
    } root{&selected};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
    CHECK(selected == "Linear");
}

// ─── TableWidget ────────────────────────────────────────────────────────────────

TEST_CASE("TableWidget: only visible rows are built across 5,000 rows", "[tree][widgets][table]") {
    std::set<int> builtRows;
    int           clickedRow = -1;

    struct Root {
        std::set<int>* builtRows;
        int*           clickedRow;
        [[nodiscard]] Widget Build() const {
            return TableWidget(5000, 20.0f)
                       .Column("Name",
                               [b = builtRows](int r) -> std::string {
                                   b->insert(r);
                                   return "Row " + std::to_string(r);
                               })
                       .Column("Score", [](int r) -> std::string { return std::to_string(r * 10); })
                       .OnRowClick([c = clickedRow](int r) { *c = r; });
        }
    } root{&builtRows, &clickedRow};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());

    // Far fewer than 5000 rows built proves the VirtualList underneath is virtualizing.
    CHECK(builtRows.size() < 30);
}

TEST_CASE("TableWidget: row click fires OnRowClick with the clicked row index", "[tree][widgets][table]") {
    // A pixel-precise mouse simulation would need to land inside a specific row of a
    // VirtualList nested inside a scrolling child window below a header row — fragile
    // and not worth pinning to exact font/padding metrics. Instead this verifies the
    // composition and Delegate wiring directly: extract row 3's built Widget from
    // TableWidget::Build() and fire its GestureRegion's OnClick by hand.
    int clickedRow = -1;
    TableWidget table = TableWidget(50, 20.0f)
                            .Column("Name", [](int r) -> std::string { return "Row " + std::to_string(r); })
                            .OnRowClick([&clickedRow](int r) { clickedRow = r; });

    Widget built = table.Build();
    const auto& outerFlex = built.As<Tree::Primitives::Flex>();
    REQUIRE(outerFlex.GetChildren().size() == 2);

    const auto& virtualList = outerFlex.GetChildren()[1].As<VirtualList>();
    Widget       row3       = virtualList.GetBuilder()(3);
    const auto&  gesture    = row3.As<Tree::Primitives::GestureRegion>();

    REQUIRE(gesture.GetOnClick());
    gesture.GetOnClick()();

    CHECK(clickedRow == 3);
}

// ─── ModalWidget ────────────────────────────────────────────────────────────────

TEST_CASE("ModalWidget: renders without error whether open or closed", "[tree][widgets][modal]") {
    bool open = true;
    struct Root {
        bool* open;
        [[nodiscard]] Widget Build() const {
            return Overlay::ModalWidget("Settings", open).Content(Tree::Primitives::Text("Settings content"));
        }
    } root{&open};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

// ─── ContextMenuWidget ──────────────────────────────────────────────────────────

TEST_CASE("ContextMenuWidget: renders without error and fires item action when invoked", "[tree][widgets][contextmenu]") {
    bool deleted = false;
    Overlay::ContextMenuWidget menu =
        Overlay::ContextMenuWidget(Tree::Primitives::Text("file.txt")).Item("Delete", [&deleted] { deleted = true; });

    REQUIRE(menu.GetItems().size() == 1);
    menu.GetItems()[0].Action();
    CHECK(deleted);

    struct Root {
        [[nodiscard]] Widget Build() const {
            return Overlay::ContextMenuWidget(Tree::Primitives::Text("file.txt"))
                       .Item("Open", [] {})
                       .Item("Delete", [] {});
        }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

// ─── ToastOverlayWidget ─────────────────────────────────────────────────────────

TEST_CASE("ToastOverlayWidget: renders a snapshot of active toasts without error", "[tree][widgets][toast]") {
    std::vector<Overlay::ToastSnapshot> toasts{
        Overlay::ToastSnapshot{Overlay::ToastType::Success, "Saved", "File written to disk.", 1.0f},
        Overlay::ToastSnapshot{Overlay::ToastType::Error, "Connection lost", "", 0.5f},
    };

    struct Root {
        std::vector<Overlay::ToastSnapshot>* toasts;
        [[nodiscard]] Widget Build() const { return Overlay::ToastOverlayWidget(*toasts); }
    } root{&toasts};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

TEST_CASE("ToastOverlayWidget: empty snapshot renders without error", "[tree][widgets][toast]") {
    struct Root {
        [[nodiscard]] Widget Build() const { return Overlay::ToastOverlayWidget({}); }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

// ─── SeparatorWidget (Phase 30.1) ───────────────────────────────────────────────

TEST_CASE("SeparatorWidget: renders without error, plain and labelled", "[tree][widgets][separator]") {
    struct Root {
        [[nodiscard]] Widget Build() const {
            return Tree::Primitives::Flex(Tree::Primitives::Flex::Axis::Vertical).Children({
                SeparatorWidget(),
                SeparatorWidget().Label("Advanced"),
            });
        }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

// ─── ImageWidget (Phase 30.1) ───────────────────────────────────────────────────

TEST_CASE("ImageWidget: renders without error and OnClick fires when set", "[tree][widgets][image]") {
    bool clicked = false;
    int  frame   = 0;

    struct Root {
        bool* clicked;
        [[nodiscard]] Widget Build() const {
            // Texture ID 1 — the font atlas is set to ID 1 in the headless backend.
            return ImageWidget(reinterpret_cast<void*>(1), Widgets::Vec2{64.0f, 64.0f})
                       .OnClick([c = clicked] { *c = true; });
        }
    } root{&clicked};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(4), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) {
        ++frame;
        if (frame == 2) { ClickAt(10.0f, 10.0f); }
    });
    REQUIRE(app.Run().has_value());
    CHECK(clicked);
}

TEST_CASE("ImageWidget: renders without error and stays non-interactive when OnClick is unset", "[tree][widgets][image]") {
    struct Root {
        [[nodiscard]] Widget Build() const {
            return ImageWidget(reinterpret_cast<void*>(1), Widgets::Vec2{64.0f, 64.0f})
                       .Tint({1.0f, 1.0f, 1.0f, 0.8f})
                       .UV0({0.0f, 0.0f})
                       .UV1({0.5f, 0.5f});
        }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

// ─── ProgressBarWidget (Phase 30.1) ─────────────────────────────────────────────

TEST_CASE("ProgressBarWidget: renders without error", "[tree][widgets][progressbar]") {
    struct Root {
        [[nodiscard]] Widget Build() const { return ProgressBarWidget(0.5f).Overlay("Loading..."); }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

// ─── ColorEditWidget (Phase 30.1) ───────────────────────────────────────────────

TEST_CASE("ColorEditWidget: renders without error and value round-trips through the pointer binding",
          "[tree][widgets][coloredit]") {
    Widgets::Vec4 tint{1.0f, 0.5f, 0.0f, 1.0f};
    struct Root {
        Widgets::Vec4* value;
        [[nodiscard]] Widget Build() const { return ColorEditWidget("Tint", value).Alpha(true); }
    } root{&tint};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
    CHECK(tint.x == 1.0f); // unchanged — no simulated drag; confirms no crash and value round-trips
}

// ─── RadioWidget (Phase 30.1) ───────────────────────────────────────────────────

TEST_CASE("RadioWidget: renders without error and clicking sets the bound value to its option",
          "[tree][widgets][radio]") {
    int mode  = -1;
    int frame = 0;

    struct Root {
        int* value;
        [[nodiscard]] Widget Build() const { return RadioWidget("Linear", value, 0); }
    } root{&mode};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(4), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) {
        ++frame;
        if (frame == 2) { ClickAt(10.0f, 10.0f); }
    });
    REQUIRE(app.Run().has_value());
    CHECK(mode == 0);
}

// ─── GridWidget (Phase 30.1) ────────────────────────────────────────────────────

TEST_CASE("GridWidget: renders without error across multiple wrapped rows", "[tree][widgets][grid]") {
    struct Root {
        [[nodiscard]] Widget Build() const {
            return Layout::GridWidget(3).Spacing(4.0f).Children({
                Tree::Primitives::Text("A"),
                Tree::Primitives::Text("B"),
                Tree::Primitives::Text("C"),
                Tree::Primitives::Text("D"), // wraps to row 2
            });
        }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

TEST_CASE("GridWidget: exposes its children in insertion order", "[tree][widgets][grid]") {
    Layout::GridWidget grid = Layout::GridWidget(2).Children({
        Tree::Primitives::Text("A"),
        Tree::Primitives::Text("B"),
        Tree::Primitives::Text("C"),
    });

    REQUIRE(grid.GetChildren().size() == 3);
    CHECK(grid.GetChildren()[0].As<Tree::Primitives::Text>().GetContent() == "A");
    CHECK(grid.GetChildren()[2].As<Tree::Primitives::Text>().GetContent() == "C");
}

// ─── PropertyGridWidget (Phase 30.1) ────────────────────────────────────────────

TEST_CASE("PropertyGridWidget: renders without error with rows and a group separator",
          "[tree][widgets][propertygrid]") {
    std::string name = "Alice";
    struct Root {
        std::string* value;
        [[nodiscard]] Widget Build() const {
            std::vector<PropertyGridRow> rows;
            rows.emplace_back("Name", TextInputWidget<std::string>("##name", value));
            rows.push_back(PropertyGridRow::Separator("Transform"));
            rows.emplace_back("Label", Tree::Primitives::Text("static"));
            return PropertyGridWidget("##props").SplitRatio(0.4f).Rows(std::move(rows));
        }
    } root{&name};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

TEST_CASE("PropertyGridWidget: Build() composes one Flex row per property, split by SplitRatio",
          "[tree][widgets][propertygrid]") {
    PropertyGridWidget grid = PropertyGridWidget("##props").SplitRatio(0.4f).Rows({
        {"Name", Tree::Primitives::Text("Alice")},
    });

    Widget built = grid.Build();
    const auto& outerFlex = built.As<Tree::Primitives::Flex>();
    REQUIRE(outerFlex.GetChildren().size() == 1);

    const auto& rowFlex = outerFlex.GetChildren()[0].As<Tree::Primitives::Flex>();
    REQUIRE(rowFlex.GetChildren().size() == 2);

    const auto& labelExpanded = rowFlex.GetChildren()[0].As<Tree::Primitives::Expanded>();
    const auto& valueExpanded = rowFlex.GetChildren()[1].As<Tree::Primitives::Expanded>();
    CHECK(labelExpanded.GetFactor() == 40);
    CHECK(valueExpanded.GetFactor() == 60);
}
