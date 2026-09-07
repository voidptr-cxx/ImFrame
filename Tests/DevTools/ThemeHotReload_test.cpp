/**
 * @file     ThemeHotReload_test.cpp
 * @brief    Unit tests for ImFrame::DevTools::ThemeHotReload
 *
 * @internal
 * Each test that exercises file-watching creates a temp directory, writes a
 * `.toml` file, then calls Poll() after a brief sleep to allow the OS watcher
 * to detect the change. The sleep is capped at 2 s via WaitForPending().
 *
 * Tests that only exercise the TOML parser call LoadFile indirectly by writing
 * a file before Watch() is called, then touching (re-writing) the file inside
 * the watched directory to trigger the event.
 *
 * Compiled only when IMF_DEV_TOOLS is defined (see Tests/CMakeLists.txt).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-13
 * @version  1.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#if defined(IMF_DEV_TOOLS)

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "ImFrame/DevTools/ThemeHotReload.hpp"
#include "ImFrame/Theme/Themes/Dracula.hpp"
#include "ImFrame/Utility/Directory.hpp"
#include "ImFrame/Utility/File.hpp"
#include "ImFrame/Utility/Path.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

using namespace ImFrame;
using namespace ImFrame::Utility;
using namespace ImFrame::DevTools;
using Catch::Approx;
using namespace std::chrono_literals;

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace {

struct TempDir {
    Path path;

    TempDir()
    {
        auto base = Path{std::filesystem::temp_directory_path()};
        auto unique = base / ("imframe_hotreload_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        (void)Directory::CreateAll(unique);
        path = unique;
    }

    ~TempDir()
    {
        (void)Directory::Delete(path, /*recursive=*/true);
    }
};

// Polls until TakePending() returns a value or maxWaitMs elapses.
std::optional<Theme::Theme> WaitForPending(ThemeHotReload& hr, int maxWaitMs = 2000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxWaitMs);
    while (std::chrono::steady_clock::now() < deadline) {
        hr.Poll();
        if (auto t = hr.TakePending()) {
            return t;
        }
        std::this_thread::sleep_for(50ms);
    }
    return std::nullopt;
}

} // namespace

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("ThemeHotReload: TakePending returns nullopt before any file change",
          "[unit][devtools]")
{
    TempDir dir;
    ThemeHotReload hr;
    hr.SetBaseTheme(Themes::Dracula);
    hr.Watch(dir.path);

    hr.Poll();
    REQUIRE_FALSE(hr.TakePending().has_value());
}

TEST_CASE("ThemeHotReload: TOML file change sets pending theme with overridden accent",
          "[unit][devtools]")
{
    TempDir dir;
    ThemeHotReload hr;
    hr.SetBaseTheme(Themes::Dracula);
    hr.Watch(dir.path);

    const Path tomlPath = dir.path / "custom.toml";
    const std::string toml =
        "[colors]\n"
        "accentDefault = [1.0, 0.0, 0.0, 1.0]\n";

    (void)File::Write(tomlPath, toml);
    std::this_thread::sleep_for(100ms);

    const auto pending = WaitForPending(hr);

    REQUIRE(pending.has_value());
    // The accent should be pure red [1, 0, 0, 1].
    const auto& accent = pending->accentDefault;
    CHECK(accent.r == Catch::Approx(1.0f));
    CHECK(accent.g == Catch::Approx(0.0f));
    CHECK(accent.b == Catch::Approx(0.0f));
    CHECK(accent.a == Catch::Approx(1.0f));
}

TEST_CASE("ThemeHotReload: non-.toml files do not trigger a reload", "[unit][devtools]")
{
    TempDir dir;
    ThemeHotReload hr;
    hr.SetBaseTheme(Themes::Dracula);
    hr.Watch(dir.path);

    const Path txtPath = dir.path / "notes.txt";
    (void)File::Write(txtPath, "some text\n");
    std::this_thread::sleep_for(150ms);

    hr.Poll();
    REQUIRE_FALSE(hr.TakePending().has_value());
}

TEST_CASE("ThemeHotReload: unrecognised TOML keys are silently ignored",
          "[unit][devtools]")
{
    TempDir dir;
    ThemeHotReload hr;
    hr.SetBaseTheme(Themes::Dracula);
    hr.Watch(dir.path);

    const Path tomlPath = dir.path / "partial.toml";
    const std::string toml =
        "[colors]\n"
        "unknownKey = [0.5, 0.5, 0.5, 1.0]\n"
        "statusSuccess = [0.0, 1.0, 0.0, 1.0]\n";

    (void)File::Write(tomlPath, toml);
    std::this_thread::sleep_for(100ms);

    const auto pending = WaitForPending(hr);
    REQUIRE(pending.has_value());

    // statusSuccess should be overridden; accentDefault retains Dracula value.
    const auto& success = pending->statusSuccess;
    CHECK(success.g == Catch::Approx(1.0f));
}

TEST_CASE("ThemeHotReload: TakePending clears the pending state", "[unit][devtools]")
{
    TempDir dir;
    ThemeHotReload hr;
    hr.SetBaseTheme(Themes::Dracula);
    hr.Watch(dir.path);

    const Path tomlPath = dir.path / "test.toml";
    (void)File::Write(tomlPath, "[colors]\naccentDefault = [0.0, 1.0, 0.0, 1.0]\n");
    std::this_thread::sleep_for(100ms);

    const auto first = WaitForPending(hr);
    REQUIRE(first.has_value());

    // Second TakePending should return nothing since we already consumed it.
    hr.Poll();
    REQUIRE_FALSE(hr.TakePending().has_value());
}

#endif // defined(IMF_DEV_TOOLS)
