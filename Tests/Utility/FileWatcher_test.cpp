/**
 * @file     FileWatcher_test.cpp
 * @brief    Unit tests for ImFrame::Utility::FileWatcher
 *
 * @internal
 * Each test creates a temp directory, registers a watch on it, performs
 * a filesystem operation, sleeps briefly to allow the background thread
 * time to detect the change, then calls Poll().
 *
 * The 100ms sleep is conservative — platform watchers typically report
 * within a few milliseconds — but is required for test reliability on
 * loaded CI machines.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/FileWatcher.hpp"
#include "ImFrame/Utility/File.hpp"
#include "ImFrame/Utility/Directory.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

using namespace ImFrame::Utility;
using namespace std::chrono_literals;

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace {

struct TempDir {
    Path path;

    TempDir()
    {
        auto base  = Path{std::filesystem::temp_directory_path()};
        auto unique = base / ("imframe_watchtest_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        (void)Directory::CreateAll(unique);
        path = unique;
    }

    ~TempDir()
    {
        (void)Directory::Delete(path, /*recursive=*/true);
    }
};

/// Poll up to maxWaitMs for at least one event matching predicate.
template<typename Pred>
bool WaitForEvent(FileWatcher& watcher, Pred&& pred, int maxWaitMs = 2000)
{
    bool found = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxWaitMs);
    while (!found && std::chrono::steady_clock::now() < deadline) {
        watcher.Poll([&](const FileEvent& ev) {
            if (pred(ev)) found = true;
        });
        if (!found) std::this_thread::sleep_for(50ms);
    }
    return found;
}

} // anonymous namespace

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("FileWatcher::Watch returns a valid handle for an existing directory", "[unit]")
{
    TempDir tmp;
    FileWatcher watcher;

    auto result = watcher.Watch(tmp.path);
    REQUIRE(result.has_value());
    REQUIRE(*result != InvalidWatchHandle);

    watcher.Unwatch(*result);
}

TEST_CASE("FileWatcher::Watch returns WatchFailed for a non-existent path", "[unit]")
{
    TempDir tmp;
    FileWatcher watcher;
    Path ghost = tmp.path / "does_not_exist";

    auto result = watcher.Watch(ghost);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ImFrame::Error::WatchFailed);
}

TEST_CASE("FileWatcher detects a file write as a Modified or Created event", "[unit]")
{
    TempDir tmp;
    FileWatcher watcher;

    auto handle = watcher.Watch(tmp.path);
    REQUIRE(handle.has_value());

    Path target = tmp.path / "watched.txt";
    REQUIRE(File::Write(target, "trigger").has_value());

    bool detected = WaitForEvent(watcher, [&](const FileEvent& ev) {
        return ev.Type == FileChangeType::Modified ||
               ev.Type == FileChangeType::Created;
    });

    REQUIRE(detected);
    watcher.Unwatch(*handle);
}

TEST_CASE("FileWatcher detects deletion as a Deleted event", "[unit]")
{
    TempDir tmp;
    // Pre-create the file before registering the watch so the test
    // observes the Delete rather than the Create.
    Path target = tmp.path / "to_delete.txt";
    REQUIRE(File::Write(target, "bye").has_value());

    FileWatcher watcher;
    auto handle = watcher.Watch(tmp.path);
    REQUIRE(handle.has_value());

    REQUIRE(File::Delete(target).has_value());

    bool detected = WaitForEvent(watcher, [](const FileEvent& ev) {
        return ev.Type == FileChangeType::Deleted;
    });

    REQUIRE(detected);
    watcher.Unwatch(*handle);
}

TEST_CASE("FileWatcher::Unwatch suppresses subsequent events", "[unit]")
{
    TempDir tmp;
    FileWatcher watcher;

    auto handle = watcher.Watch(tmp.path);
    REQUIRE(handle.has_value());

    // Unwatch before any filesystem operation.
    watcher.Unwatch(*handle);

    // Drain any events that arrived between Watch and Unwatch.
    std::this_thread::sleep_for(50ms);
    watcher.Poll([](const FileEvent&) {});

    // Now perform a filesystem change — no events should arrive.
    REQUIRE(File::Write(tmp.path / "suppressed.txt", "data").has_value());
    std::this_thread::sleep_for(150ms);

    uint32_t count = 0;
    watcher.Poll([&](const FileEvent&) { ++count; });

    REQUIRE(count == 0);
}
