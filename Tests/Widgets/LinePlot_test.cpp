/**
 * @file     LinePlot_test.cpp
 * @brief    Unit tests for Widgets::LinePlot
 *
 * @internal
 * All tests run inside a headless ImGui + ImPlot context via
 * TestHeadlessBackend. PlotContext::Init() and Shutdown() are invoked
 * automatically by Application::Run(), so tests exercise the full lifecycle
 * without any per-test setup.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-10
 * @version  1.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "../App/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Theme/Theme.hpp"
#include "ImFrame/Theme/Themes/Dracula.hpp"
#include "ImFrame/Widgets/LinePlot.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>

using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;
using ImFrame::Widgets::LinePlot;

TEST_CASE("LinePlot with valid data does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    const std::array<double, 5> xs{0.0, 1.0, 2.0, 3.0, 4.0};
    const std::array<double, 5> ys{0.0, 1.0, 4.0, 9.0, 16.0};

    application.OnUi([&] {
        LinePlot("##test_line")
            .Size(400.0f, 300.0f)
            .Series("quadratic", xs, ys)
            .Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("LinePlot renders multiple series without crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    // Data moved inside the lambda as static const to stay within Delegate SBO (16 bytes).
    application.OnUi([&] {
        static const std::array<double, 4> xs {1.0, 2.0, 3.0, 4.0};
        static const std::array<double, 4> ysA{1.0, 2.0, 3.0, 4.0};
        static const std::array<double, 4> ysB{4.0, 3.0, 2.0, 1.0};
        LinePlot("##multi_series")
            .Size(400.0f, 300.0f)
            .XLabel("x")
            .YLabel("y")
            .Series("ascending",  xs, ysA)
            .Series("descending", xs, ysB)
            .Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("LinePlot with theme applied does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(2);
    Application application(std::move(app));
    application.WithTheme(ImFrame::Themes::Dracula);

    const std::array<double, 3> xs{0.0, 1.0, 2.0};
    const std::array<double, 3> ys{0.0, 0.5, 1.0};

    application.OnUi([&] {
        LinePlot("##themed").Series("data", xs, ys).Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("LinePlot Show clears series after each frame", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(2);
    Application application(std::move(app));

    LinePlot plot("##clear_test");
    int frameCount = 0;

    application.OnUi([&] {
        // Data as static const to stay within Delegate SBO (16 bytes): only plot + frameCount captured.
        static const std::array<double, 3> xs{0.0, 1.0, 2.0};
        static const std::array<double, 3> ys{0.0, 1.0, 4.0};
        ++frameCount;
        if (frameCount == 1) {
            plot.Series("data", xs, ys);
        }
        // Frame 2: no Series() call — series list is empty after first Show()
        plot.Show();
    });

    REQUIRE(application.Run().has_value());
}

TEST_CASE("LinePlot with empty spans does not crash", "[unit]") {
    auto app = std::make_unique<TestHeadlessBackend>(1);
    Application application(std::move(app));

    application.OnUi([&] {
        LinePlot("##empty")
            .Series("none", std::span<const double>{}, std::span<const double>{})
            .Show();
    });

    REQUIRE(application.Run().has_value());
}
