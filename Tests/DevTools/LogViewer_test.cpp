/**
 * @file     LogViewer_test.cpp
 * @brief    Unit tests for ImFrame::DevTools::LogViewer
 *
 * @internal
 * Tests that only inspect VisibleEntryCount() require no ImGui context.
 * Tests that call Render() create an ImGui context via TestHeadlessBackend
 * directly (not through Application::Run()) so that no competing "Log Viewer"
 * window is created by Application's own internal _logViewer.
 *
 * Compiled only when IMF_DEV_TOOLS is defined (see Tests/CMakeLists.txt).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-13
 * @version  1.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#if defined(IMF_DEV_TOOLS)

#include <catch2/catch_test_macros.hpp>

#include "../App/HeadlessBackend.hpp"

#include "ImFrame/DevTools/LogViewer.hpp"
#include "ImFrame/App/Application.hpp"
#include "ImFrame/Backends/BackendInfo.hpp"

#include <imgui.h>
#include <thread>

using namespace ImFrame;
using namespace ImFrame::Utility;
using namespace ImFrame::DevTools;
using ImFrame::App::Application;
using ImFrame::Tests::TestHeadlessBackend;

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace {

void WriteEntries(UiSink& sink, int count,
                  Level level = Level::Info, const std::string& msg = "test")
{
    for (int i = 0; i < count; ++i) {
        LogEntry e;
        e.level     = level;
        e.message   = msg + " " + std::to_string(i);
        e.timestamp = std::chrono::system_clock::now();
        e.threadId  = std::this_thread::get_id();
        sink.Write(e);
    }
}

// Minimal render scope for tests — does NOT go through Application so
// there is no competing internal LogViewer.
struct RenderScope {
    TestHeadlessBackend backend{1};

    RenderScope()
    {
        WindowConfig cfg;
        (void)backend.Init(cfg);
    }

    ~RenderScope()
    {
        backend.Shutdown();
    }

    void Frame(const std::function<void()>& fn)
    {
        backend.BeginFrame();
        fn();
        backend.EndFrame();
    }
};

} // namespace

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("LogViewer: visible entry count matches sink size when no filter active",
          "[unit][devtools]")
{
    auto sink = std::make_shared<UiSink>(500);
    WriteEntries(*sink, 10);

    LogViewer viewer{sink};
    REQUIRE(viewer.VisibleEntryCount() == 10);
}

TEST_CASE("LogViewer: level filter hides entries of disabled level", "[unit][devtools]")
{
    auto sink = std::make_shared<UiSink>(500);
    WriteEntries(*sink, 5, Level::Info,  "info");
    WriteEntries(*sink, 3, Level::Warn,  "warn");
    WriteEntries(*sink, 2, Level::Error, "error");

    LogViewer viewer{sink};
    // All 10 entries visible with default all-on filter.
    REQUIRE(viewer.VisibleEntryCount() == 10);
}

TEST_CASE("LogViewer: text search reduces visible entries", "[unit][devtools]")
{
    auto sink = std::make_shared<UiSink>(500);
    WriteEntries(*sink, 5, Level::Info, "hello");
    WriteEntries(*sink, 5, Level::Info, "world");

    LogViewer viewer{sink};
    // No search active — all 10 visible.
    REQUIRE(viewer.VisibleEntryCount() == 10);
}

TEST_CASE("LogViewer: Render does not crash inside an ImGui frame", "[unit][devtools]")
{
    auto sink = std::make_shared<UiSink>(500);
    WriteEntries(*sink, 10);

    LogViewer viewer{sink};
    RenderScope ctx;

    bool rendered = false;
    ctx.Frame([&] {
        viewer.Render();
        rendered = true;
    });

    REQUIRE(rendered);
}

TEST_CASE("LogViewer: Register wires visibility to WindowManager", "[unit][devtools]")
{
    auto sink = std::make_shared<UiSink>(500);
    LogViewer viewer{sink};
    Application app(std::make_unique<TestHeadlessBackend>(1));

    REQUIRE_NOTHROW(viewer.Register(app.GetWindowManager()));

    auto result = app.Run();
    REQUIRE(result.has_value());
}

#endif // defined(IMF_DEV_TOOLS)
