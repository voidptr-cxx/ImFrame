/**
 * @file     Directory_test.cpp
 * @brief    Unit tests for ImFrame::Utility::Directory free functions
 *
 * @internal
 * All tests use a unique temporary directory cleaned up in the fixture.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/Directory.hpp"
#include "ImFrame/Utility/File.hpp"

#include <filesystem>
#include <string>
#include <atomic>
#include <chrono>

using namespace ImFrame::Utility;

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace {

struct TempDir {
    Path path;

    TempDir()
    {
        auto base  = Path{std::filesystem::temp_directory_path()};
        auto unique = base / ("imframe_dirtest_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        (void)Directory::CreateAll(unique);
        path = unique;
    }

    ~TempDir()
    {
        (void)Directory::Delete(path, /*recursive=*/true);
    }
};

} // anonymous namespace

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("Directory::CreateAll creates a deeply nested path in one call", "[unit]")
{
    TempDir tmp;
    Path nested = tmp.path / "a" / "b" / "c";

    REQUIRE(Directory::CreateAll(nested).has_value());
    REQUIRE(Directory::Exists(nested));
}

TEST_CASE("Directory::Exists returns false for a non-existent path", "[unit]")
{
    TempDir tmp;
    REQUIRE_FALSE(Directory::Exists(tmp.path / "ghost"));
}

TEST_CASE("Directory::List returns only immediate children", "[unit]")
{
    TempDir tmp;
    // Create 2 files and 1 sub-directory.
    REQUIRE(File::Write(tmp.path / "a.txt", "a").has_value());
    REQUIRE(File::Write(tmp.path / "b.txt", "b").has_value());
    REQUIRE(Directory::Create(tmp.path / "subdir").has_value());

    auto result = Directory::List(tmp.path);
    REQUIRE(result.has_value());
    REQUIRE(result->size() == 3);
}

TEST_CASE("Directory::ListRecursive returns all descendants", "[unit]")
{
    TempDir tmp;
    REQUIRE(Directory::CreateAll(tmp.path / "x" / "y").has_value());
    REQUIRE(File::Write(tmp.path / "x" / "1.txt", "1").has_value());
    REQUIRE(File::Write(tmp.path / "x" / "y" / "2.txt", "2").has_value());

    auto result = Directory::ListRecursive(tmp.path);
    REQUIRE(result.has_value());
    // Expect: x/, x/y/, x/1.txt, x/y/2.txt = 4 entries
    REQUIRE(result->size() == 4);
}

TEST_CASE("Directory::Walk (void visitor) visits every entry", "[unit]")
{
    TempDir tmp;
    REQUIRE(File::Write(tmp.path / "f1.txt", "1").has_value());
    REQUIRE(File::Write(tmp.path / "f2.txt", "2").has_value());
    REQUIRE(File::Write(tmp.path / "f3.txt", "3").has_value());

    int count = 0;
    auto result = Directory::Walk(tmp.path, [&](const Path&) { ++count; });

    REQUIRE(result.has_value());
    REQUIRE(count == 3);
}

TEST_CASE("Directory::Walk (bool visitor) stops when visitor returns false", "[unit]")
{
    TempDir tmp;
    REQUIRE(File::Write(tmp.path / "a.txt", "a").has_value());
    REQUIRE(File::Write(tmp.path / "b.txt", "b").has_value());
    REQUIRE(File::Write(tmp.path / "c.txt", "c").has_value());

    int count = 0;
    // Explicitly typed to resolve Walk overload ambiguity (void vs bool visitor).
    std::function<bool(const Path&)> stopVisitor = [&](const Path&) -> bool {
        ++count;
        return false; // stop immediately
    };
    auto result = Directory::Walk(tmp.path, stopVisitor);

    REQUIRE(result.has_value());
    REQUIRE(count == 1);
}

TEST_CASE("Directory::Delete with recursive=true removes non-empty directory", "[unit]")
{
    TempDir tmp;
    Path subdir = tmp.path / "to_delete";
    REQUIRE(Directory::Create(subdir).has_value());
    REQUIRE(File::Write(subdir / "file.txt", "content").has_value());

    REQUIRE(Directory::Delete(subdir, /*recursive=*/true).has_value());
    REQUIRE_FALSE(Directory::Exists(subdir));
}
