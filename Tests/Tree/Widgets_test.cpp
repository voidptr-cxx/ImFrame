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
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Overlay/ContextMenu.hpp"
#include "ImFrame/Overlay/Modal.hpp"
#include "ImFrame/Overlay/Toast.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/GestureRegion.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Tree/VirtualList.hpp"
#include "ImFrame/Widgets/Button.hpp"
#include "ImFrame/Widgets/Checkbox.hpp"
#include "ImFrame/Widgets/Combo.hpp"
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
            return Widget(ButtonWidget("Save").OnClick([c = clicked] { *c = true; }));
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
            return Widget(ButtonWidget("Save").Disabled().OnClick([c = clicked] { *c = true; }));
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
            return Widget(CheckboxWidget("Wireframe", value).OnChange([f = onChangeFired](bool) { *f = true; }));
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
            return Widget(SliderWidget<float>("Volume", value, 0.0f, 1.0f).Format("%.2f"));
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
        [[nodiscard]] Widget Build() const { return Widget(SliderWidget<int>("Level", value, 0, 10)); }
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
            return Widget(TextInputWidget<std::string>("Name", value).Hint("Enter your name"));
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
            return Widget(ComboWidget<std::string>("Filter", value, std::span<const std::string>(modes)));
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
            return Widget(TableWidget(5000, 20.0f)
                              .Column("Name",
                                      [b = builtRows](int r) -> std::string {
                                          b->insert(r);
                                          return "Row " + std::to_string(r);
                                      })
                              .Column("Score", [](int r) -> std::string { return std::to_string(r * 10); })
                              .OnRowClick([c = clickedRow](int r) { *c = r; }));
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
            return Widget(Overlay::ModalWidget("Settings", open).Content(Widget(Tree::Primitives::Text("Settings content"))));
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
        Overlay::ContextMenuWidget(Widget(Tree::Primitives::Text("file.txt"))).Item("Delete", [&deleted] { deleted = true; });

    REQUIRE(menu.GetItems().size() == 1);
    menu.GetItems()[0].Action();
    CHECK(deleted);

    struct Root {
        [[nodiscard]] Widget Build() const {
            return Widget(Overlay::ContextMenuWidget(Widget(Tree::Primitives::Text("file.txt")))
                              .Item("Open", [] {})
                              .Item("Delete", [] {}));
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
        [[nodiscard]] Widget Build() const { return Widget(Overlay::ToastOverlayWidget(*toasts)); }
    } root{&toasts};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}

TEST_CASE("ToastOverlayWidget: empty snapshot renders without error", "[tree][widgets][toast]") {
    struct Root {
        [[nodiscard]] Widget Build() const { return Widget(Overlay::ToastOverlayWidget({})); }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}
