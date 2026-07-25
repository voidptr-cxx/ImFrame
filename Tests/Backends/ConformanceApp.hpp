/**
 * @file     ConformanceApp.hpp
 * @brief    Shared widget-tree conformance fixture, reused across every backend's conformance test file
 *
 * `ConformanceApp` is a `Tree::Component` touching a representative sample of
 * the widget library still available after Phase 30.2's deprecated-API
 * removal — interactive widgets (`ButtonWidget`, `CheckboxWidget`,
 * `SliderWidget<float>`, `TextInputWidget<std::string>`, `ComboWidget<int>`,
 * `ProgressBarWidget`, `ColorEditWidget`, `RadioWidget`), the two remaining
 * composite widgets (`GridWidget`, `TableWidget`), and the core layout
 * primitives (`Flex`, `Box`, `Expanded`) that every other primitive composes
 * with. It is not an exhaustive enumeration of every `Tree::Primitives::*`
 * type (e.g. `VirtualList`'s lazy/unbounded rows and `Portal` are out of
 * scope, matching the same documented boundary `WidgetTestDriver::
 * FindDescendant` already draws) — it is a representative cross-section
 * deliberately sized to actually paint pixels through a real backend's
 * `Layout()`/`Paint()` pipeline without ballooning render time per backend
 * per theme per layout size.
 *
 * @internal
 * This is test-only fixture code (not shipped, no DOC_STANDARDS `@internal`
 * file-header exemption needed since it never leaves `Tests/`). Interactive
 * widgets bind to `mutable` members rather than external caller-owned
 * pointers (the codebase's usual convention, e.g. `Tests/Tree/
 * WidgetTestDriver_test.cpp`'s `LabelledCheckbox`) because this fixture is
 * self-contained — no external test code needs to observe or mutate its
 * bound state, so there is no caller to own the pointee. `Build() const`
 * requires this: `mutable` strips constness from a member's address even
 * inside a const method, giving `SliderWidget`/`CheckboxWidget`/etc. the
 * real, live pointers they require without a `const_cast`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-15
 * @version  2.9.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Layout/Grid.hpp"
#include "ImFrame/Tree/Element.hpp"
#include "ImFrame/Tree/Primitives/Box.hpp"
#include "src/Rendering/Renderers/ImGuiCompatRenderer.hpp"
#include "ImFrame/Tree/Primitives/Expanded.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Widgets/Button.hpp"
#include "ImFrame/Widgets/Checkbox.hpp"
#include "ImFrame/Widgets/ColorEdit.hpp"
#include "ImFrame/Widgets/Combo.hpp"
#include "ImFrame/Widgets/ProgressBar.hpp"
#include "ImFrame/Widgets/Radio.hpp"
#include "ImFrame/Widgets/Slider.hpp"
#include "ImFrame/Widgets/Table.hpp"
#include "ImFrame/Widgets/TextInput.hpp"

#include <imgui.h>

#include <array>
#include <span>
#include <string>

namespace ImFrame::ConformanceTest {

/// Touches a representative cross-section of the widget library through a real Layout()/Paint() pass.
class ConformanceApp {
public:
    [[nodiscard]] Tree::Widget Build() const {
        using namespace ImFrame::Tree;
        using namespace ImFrame::Tree::Primitives;
        using namespace ImFrame::Widgets;

        return Flex(Flex::Axis::Vertical).Gap(4.0f).Children({
            Text("ImFrame Conformance"),
            Flex(Flex::Axis::Horizontal).Gap(4.0f).Children({
                ButtonWidget("Button"),
                CheckboxWidget("Checkbox", &_checked),
                Expanded(SliderWidget<float>("Slider", &_sliderValue, 0.0f, 1.0f)),
            }),
            TextInputWidget<std::string>("Input", &_inputValue),
            ComboWidget<int>("Combo", &_comboValue, std::span<const int>(_comboItems)),
            ProgressBarWidget(0.65f).Overlay("65%"),
            ColorEditWidget("Color", &_colorValue).Alpha(true),
            Flex(Flex::Axis::Horizontal).Gap(4.0f).Children({
                RadioWidget("A", &_radioValue, 0),
                RadioWidget("B", &_radioValue, 1),
                RadioWidget("C", &_radioValue, 2),
            }),
            Box().Padding(Widgets::EdgeInsets::All(4.0f))
                 .Background({0.2f, 0.2f, 0.25f, 1.0f})
                 .Radius(3.0f)
                 .Child(Layout::GridWidget(2).Spacing(4.0f).Children({
                     Text("Grid A"), Text("Grid B"),
                     Text("Grid C"), Text("Grid D"),
                 })),
            TableWidget(3, 20.0f)
                .Column("Name", [](int r) { return "Row " + std::to_string(r); })
                .Column("Value", [](int r) { return std::to_string(r * 10); }),
        });
    }

private:
    mutable bool        _checked     = true;
    mutable float        _sliderValue = 0.5f;
    mutable std::string  _inputValue  = "conformance";
    mutable int          _comboValue  = 0;
    mutable Widgets::Vec4 _colorValue{0.2f, 0.6f, 0.9f, 1.0f};
    mutable int          _radioValue  = 1;

    static constexpr std::array<int, 3> _comboItems{0, 1, 2};
};

/**
 * @brief    Mounts, lays out, and paints a fresh `ConformanceApp` inside its own ImGui window.
 *
 * Call between a backend's `BeginFrame()`/`EndFrame()`. Each call is a
 * self-contained single-frame snapshot (mount, layout, paint, unmount) — no
 * dirty-tracking benefit is needed across the one-shot renders a conformance
 * test performs per (theme, size) combination.
 *
 * @param[in] width   Window/render-target width in pixels.
 * @param[in] height  Window/render-target height in pixels.
 */
inline void RenderConformanceApp(int width, int height) {
    ConformanceApp app;
    Tree::Widget    widget  = app.Build();
    auto             element = widget.CreateElement();
    element->Mount(nullptr, 0, widget);

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({static_cast<float>(width), static_cast<float>(height)});
    ImGui::Begin("ConformanceApp", nullptr,
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);
    (void)element->Layout(Tree::BoxConstraints::Loose({static_cast<float>(width), static_cast<float>(height)}));

    Rendering::CommandBuffer cmd;
    element->Paint(cmd, {ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y});
    Internal::ImGuiCompatRenderer{}.Render(cmd);

    ImGui::End();

    element->Unmount();
}

} // namespace ImFrame::ConformanceTest
