/**
 * @file     Logger_test.cpp
 * @brief    Unit and thread-safety tests for the Logger and its sinks
 *
 * @internal
 * Tests verify: log entry reaches a registered sink, Shutdown() drains the
 * BackgroundWorker, FileSink rotation triggers at the byte threshold,
 * UiSink caps entries at its configured maximum, compile-time level
 * filtering produces a no-op for suppressed levels.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/Logger.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace ImFrame::Utility;
namespace fs = std::filesystem;

// ─── Helper: a recording sink ─────────────────────────────────────────────────

class RecordSink : public Sink {
public:
    void Write(const LogEntry& entry) override {
        entries.push_back(entry);
    }
    std::vector<LogEntry> entries;
};

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("Logger entry reaches a registered sink after Shutdown", "[unit]") {
    Logger logger;
    auto sink = std::make_shared<RecordSink>();
    logger.AddSink(sink);

    logger.Write(Level::Info, "hello world");
    logger.Shutdown();

    REQUIRE(sink->entries.size() == 1);
    REQUIRE(sink->entries[0].message == "hello world");
    REQUIRE(sink->entries[0].level == Level::Info);
}

TEST_CASE("Logger dispatches to multiple sinks", "[unit]") {
    Logger logger;
    auto s1 = std::make_shared<RecordSink>();
    auto s2 = std::make_shared<RecordSink>();
    logger.AddSink(s1);
    logger.AddSink(s2);

    logger.Write(Level::Debug, "multi");
    logger.Shutdown();

    REQUIRE(s1->entries.size() == 1);
    REQUIRE(s2->entries.size() == 1);
}

TEST_CASE("Logger SetMinLevel suppresses entries below the threshold", "[unit]") {
    Logger logger;
    auto sink = std::make_shared<RecordSink>();
    logger.AddSink(sink);
    logger.SetMinLevel(Level::Warn);

    logger.Write(Level::Debug, "suppressed");
    logger.Write(Level::Info,  "also suppressed");
    logger.Write(Level::Warn,  "visible");
    logger.Shutdown();

    REQUIRE(sink->entries.size() == 1);
    REQUIRE(sink->entries[0].level == Level::Warn);
}

TEST_CASE("Logger RemoveSink stops delivery to the removed sink", "[unit]") {
    Logger logger;
    auto sink = std::make_shared<RecordSink>();
    logger.AddSink(sink);
    logger.RemoveSink(sink);

    logger.Write(Level::Info, "ghost");
    logger.Shutdown();

    REQUIRE(sink->entries.empty());
}

TEST_CASE("Logger entry preserves source location", "[unit]") {
    Logger logger;
    auto sink = std::make_shared<RecordSink>();
    logger.AddSink(sink);

    logger.Write(Level::Info, "loc test");
    logger.Shutdown();

    REQUIRE(!sink->entries.empty());
    // Line number should be > 0 and file name should be non-empty
    REQUIRE(sink->entries[0].location.line() > 0);
    REQUIRE(std::string{sink->entries[0].location.file_name()}.size() > 0);
}

TEST_CASE("Logger entry captures calling thread id", "[unit][tsan]") {
    Logger logger;
    auto sink = std::make_shared<RecordSink>();
    logger.AddSink(sink);

    const auto callerThread = std::this_thread::get_id();
    logger.Write(Level::Info, "thread check");
    logger.Shutdown();

    REQUIRE(!sink->entries.empty());
    REQUIRE(sink->entries[0].threadId == callerThread);
}

TEST_CASE("Logger write from multiple threads is safe", "[unit][tsan]") {
    Logger logger;
    auto sink = std::make_shared<RecordSink>();
    logger.AddSink(sink);

    static constexpr int THREADS = 4;
    static constexpr int WRITES  = 50;

    std::vector<std::jthread> threads;
    threads.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < WRITES; ++i)
                logger.Write(Level::Debug, std::format("t{} i{}", t, i));
        });
    }
    threads.clear();
    logger.Shutdown();

    REQUIRE(sink->entries.size() == THREADS * WRITES);
}

TEST_CASE("UiSink caps entries at configured maximum", "[unit]") {
    UiSink uiSink{3};

    uiSink.Write({Level::Info, "a", {}, {}, {}});
    uiSink.Write({Level::Info, "b", {}, {}, {}});
    uiSink.Write({Level::Info, "c", {}, {}, {}});
    uiSink.Write({Level::Info, "d", {}, {}, {}});  // oldest "a" is dropped

    auto entries = uiSink.GetEntries();
    REQUIRE(entries.size() == 3);
    REQUIRE(entries[0].message == "b");
    REQUIRE(entries[2].message == "d");
}

TEST_CASE("UiSink Clear empties the buffer", "[unit]") {
    UiSink uiSink{10};
    uiSink.Write({Level::Info, "x", {}, {}, {}});
    uiSink.Clear();
    REQUIRE(uiSink.GetEntries().empty());
}

TEST_CASE("FileSink writes to a file and rotation triggers at threshold", "[unit]") {
    const auto tmpDir  = fs::temp_directory_path();
    const auto logStr  = (tmpDir / "imframe_test.log").string();
    const auto rotStr  = (tmpDir / "imframe_test.1.log").string();
    const auto logPath = Path{logStr.c_str()};
    const auto rotPath = Path{rotStr.c_str()};

    // Clean up any prior run
    fs::remove(logPath.Native());
    fs::remove(rotPath.Native());

    {
        // Small rotation threshold — a single entry should trigger it
        FileSink sink{logPath, 1}; // 1 byte threshold → rotate immediately after first write
        sink.Write({Level::Info,  "first",  {}, {}, {}});
        sink.Write({Level::Info,  "second", {}, {}, {}});
        sink.Flush();
    }

    // The rotated file must exist
    REQUIRE(fs::exists(rotPath.Native()));
    // Current log must exist too
    REQUIRE(fs::exists(logPath.Native()));

    // Clean up
    fs::remove(logPath.Native());
    fs::remove(rotPath.Native());
}

TEST_CASE("IMF_LOG_LEVEL_MIN compile-time filtering", "[unit]") {
    // IMF_LOG_LEVEL_MIN defaults to 1 (Debug) in debug builds.
    // Verify that Trace (level 0) is below the minimum in default config.
    static_assert(static_cast<int>(Level::Trace) == 0);
    static_assert(static_cast<int>(Level::Debug) == 1);
    static_assert(IMF_LOG_LEVEL_MIN >= 1, "Trace should be filtered in non-trace builds");
    SUCCEED("compile-time level filtering static assertions passed");
}
