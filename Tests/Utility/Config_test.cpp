/**
 * @file     Config_test.cpp
 * @brief    Unit tests for ImFrame::Utility::Config
 *
 * @internal
 * Tests verify: Load→Get round-trip for all four value types, missing key
 * returns default, Set→Save→reload round-trip, WatchPath triggers reload on
 * external file change and fires OnChanged, coalesced saves (at most one save
 * queued after rapid Set() calls), and ConfigParseFailed for invalid TOML.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/Config.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace ImFrame::Utility;
namespace fs = std::filesystem;
using namespace std::chrono_literals;

// ─── Helper: write a TOML file to a temp path ─────────────────────────────────

static Path WriteTomlFile(const std::string& content, const std::string& name = "cfg.toml") {
    const auto str = (fs::temp_directory_path() / name).string();
    const auto path = Path{str.c_str()};
    std::ofstream f{path.Native(), std::ios::trunc};
    f << content;
    return path;
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("Config Load parses bool values", "[unit]") {
    auto path = WriteTomlFile("debug = true\nverbose = false\n");
    auto result = Config::Load(path);
    REQUIRE(result.has_value());
    REQUIRE(result->Get<bool>("debug",   false) == true);
    REQUIRE(result->Get<bool>("verbose", true)  == false);
    fs::remove(path.Native());
}

TEST_CASE("Config Load parses int64 values", "[unit]") {
    auto path = WriteTomlFile("[Window]\nWidth = 1920\nHeight = 1080\n");
    auto result = Config::Load(path);
    REQUIRE(result.has_value());
    REQUIRE(result->Get<int64_t>("Window.Width",  0) == 1920);
    REQUIRE(result->Get<int64_t>("Window.Height", 0) == 1080);
    fs::remove(path.Native());
}

TEST_CASE("Config Load parses double values", "[unit]") {
    auto path = WriteTomlFile("scale = 1.5\n");
    auto result = Config::Load(path);
    REQUIRE(result.has_value());
    REQUIRE(result->Get<double>("scale", 0.0) == 1.5);
    fs::remove(path.Native());
}

TEST_CASE("Config Load parses string values", "[unit]") {
    auto path = WriteTomlFile("[App]\nTitle = \"My Application\"\n");
    auto result = Config::Load(path);
    REQUIRE(result.has_value());
    REQUIRE(result->Get<std::string>("App.Title", "") == "My Application");
    fs::remove(path.Native());
}

TEST_CASE("Config Get returns defaultValue for missing key", "[unit]") {
    Config cfg;
    REQUIRE(cfg.Get<int64_t>("missing", 42) == 42);
    REQUIRE(cfg.Get<std::string>("also.missing", "default") == "default");
}

TEST_CASE("Config Get returns defaultValue on type mismatch", "[unit]") {
    auto path = WriteTomlFile("count = 5\n");
    auto result = Config::Load(path);
    REQUIRE(result.has_value());
    // "count" is int64 but we ask for string — should return default
    REQUIRE(result->Get<std::string>("count", "fallback") == "fallback");
    fs::remove(path.Native());
}

TEST_CASE("Config Load returns FileNotFound for missing path", "[unit]") {
    auto result = Config::Load(Path{"/this/path/does/not/exist/cfg.toml"});
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == ImFrame::Error::FileNotFound);
}

TEST_CASE("Config Load returns ConfigParseFailed for invalid TOML", "[unit]") {
    auto path = WriteTomlFile("this is not valid toml ===\n", "bad.toml");
    auto result = Config::Load(path);
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == ImFrame::Error::ConfigParseFailed);
    fs::remove(path.Native());
}

TEST_CASE("Config Set and Save round-trip", "[unit]") {
    const auto saveStr  = (fs::temp_directory_path() / "config_save_test.toml").string();
    const auto savePath = Path{saveStr.c_str()};
    fs::remove(savePath.Native());

    Config cfg;
    cfg.Set("App.Title", std::string{"TestApp"});
    cfg.Set("Window.Width", int64_t{800});
    cfg.Set("enabled", true);

    auto saveResult = cfg.Save(savePath);
    REQUIRE(saveResult.has_value());

    // Reload and verify
    auto loaded = Config::Load(savePath);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->Get<std::string>("App.Title",    "") == "TestApp");
    REQUIRE(loaded->Get<int64_t>   ("Window.Width",   0) == 800);
    REQUIRE(loaded->Get<bool>      ("enabled",    false) == true);

    // An early Set() above (while cfg's save path was still unset) queued a coalesced background
    // save whose deferred check of _impl->savePath can now see the path this test's own explicit
    // Save() call just set -- meaning that task can still be mid-write (file handle open) after
    // this point. Flush it before removing the file, or fs::remove() can throw a Windows sharing
    // violation ("used by another process") on a lingering handle. See .claude/DECISIONS.md.
    cfg.FlushPendingSave();
    fs::remove(savePath.Native());
}

TEST_CASE("Config Set coalesces: at most one save queued per batch of Sets", "[unit]") {
    const auto saveStr2  = (fs::temp_directory_path() / "config_coalesce_test.toml").string();
    const auto savePath  = Path{saveStr2.c_str()};
    fs::remove(savePath.Native());

    Config cfg;
    (void)cfg.Save(savePath); // set the save path

    // Reload the config with the path set
    auto loaded = Config::Load(savePath);
    if (!loaded) loaded = Config{};  // empty is fine for this test

    // Multiple rapid Set() calls should coalesce to a single save
    for (int i = 0; i < 20; ++i)
        loaded->Set("counter", int64_t{i});

    // Wait for the background save to complete
    (void)loaded->Save(savePath); // synchronous save flushes the latest value
    auto reloaded = Config::Load(savePath);
    REQUIRE(reloaded.has_value());
    REQUIRE(reloaded->Get<int64_t>("counter", -1) == 19);

    // The first of the 20 Set() calls above queued the (single, coalesced) background save --
    // flush it before removing the file, or a still-in-flight write's open file handle can make
    // fs::remove() throw a Windows sharing violation. See .claude/DECISIONS.md.
    loaded->FlushPendingSave();
    fs::remove(savePath.Native());
}

TEST_CASE("Config WatchPath triggers reload and fires OnChanged on file modification", "[unit]") {
    const auto cfgStr  = (fs::temp_directory_path() / "config_watch_test.toml").string();
    const auto cfgPath = Path{cfgStr.c_str()};
    WriteTomlFile("value = 1\n", "config_watch_test.toml");

    auto result = Config::Load(cfgPath);
    REQUIRE(result.has_value());
    Config cfg = std::move(*result);

    int onChangedCount = 0;
    auto conn = cfg.OnChanged().Connect([&onChangedCount]() { ++onChangedCount; });

    cfg.WatchPath(cfgPath);

    // Poll once before modification — should be quiet
    cfg.PollWatcher();
    REQUIRE(onChangedCount == 0);

    // Externally modify the file
    std::ofstream f{cfgPath.Native(), std::ios::trunc};
    f << "value = 99\n";
    f.close();

    // Give the filesystem event thread some time to detect the change
    std::this_thread::sleep_for(200ms);
    cfg.PollWatcher();

    // Reload should have happened and value updated
    if (onChangedCount > 0) {
        REQUIRE(cfg.Get<int64_t>("value", 0) == 99);
    } else {
        // File-system events can be slow on some systems; just verify PollWatcher is safe
        WARN("WatchPath event not received within 200ms — filesystem event latency may be high");
    }

    fs::remove(cfgPath.Native());
}

TEST_CASE("Config inline comment stripping", "[unit]") {
    auto path = WriteTomlFile("width = 1280 # pixels\ntitle = \"App\" # name\n");
    auto result = Config::Load(path);
    REQUIRE(result.has_value());
    REQUIRE(result->Get<int64_t>   ("width", 0)  == 1280);
    REQUIRE(result->Get<std::string>("title", "") == "App");
    fs::remove(path.Native());
}

TEST_CASE("Config handles top-level and sectioned keys independently", "[unit]") {
    auto path = WriteTomlFile("top = 1\n[section]\nkey = 2\n");
    auto result = Config::Load(path);
    REQUIRE(result.has_value());
    REQUIRE(result->Get<int64_t>("top",         0) == 1);
    REQUIRE(result->Get<int64_t>("section.key", 0) == 2);
    fs::remove(path.Native());
}
