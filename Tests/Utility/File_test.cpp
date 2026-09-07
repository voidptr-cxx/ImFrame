/**
 * @file     File_test.cpp
 * @brief    Unit tests for ImFrame::Utility::File free functions
 *
 * @internal
 * All tests use a unique temporary directory created per test case and
 * cleaned up in the fixture. No external resources required.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/File.hpp"
#include "ImFrame/Utility/Directory.hpp"
#include "ImFrame/Core/Error.hpp"

#include <filesystem>
#include <string>

using namespace ImFrame::Utility;

// ─── Helpers ──────────────────────────────────────────────────────────────────

namespace {

/// Creates a unique temporary directory for one test and removes it on destruction.
struct TempDir {
    Path path;

    TempDir()
    {
        auto base = Path{std::filesystem::temp_directory_path()};
        auto unique = base / ("imframe_test_" + std::to_string(
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

TEST_CASE("File::Read returns FileNotFound for a missing path", "[unit]")
{
    TempDir tmp;
    Path missing = tmp.path / "does_not_exist.txt";

    auto result = File::Read(missing);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ImFrame::Error::FileNotFound);
}

TEST_CASE("File::Write and Read round-trip UTF-8 text", "[unit]")
{
    TempDir tmp;
    Path file = tmp.path / "hello.txt";
    const std::string content = "Hello, ImFrame!";

    REQUIRE(File::Write(file, content).has_value());

    auto readResult = File::Read(file);
    REQUIRE(readResult.has_value());
    REQUIRE(*readResult == content);
}

TEST_CASE("File::WriteBytes and ReadBytes produce identical byte sequences", "[unit]")
{
    TempDir tmp;
    Path file = tmp.path / "bytes.bin";
    const std::vector<std::byte> original{
        std::byte{0x00}, std::byte{0x01}, std::byte{0xFF},
        std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}
    };

    REQUIRE(File::WriteBytes(file, original).has_value());

    auto result = File::ReadBytes(file);
    REQUIRE(result.has_value());
    REQUIRE(*result == original);
}

TEST_CASE("File::Append grows file content correctly", "[unit]")
{
    TempDir tmp;
    Path file = tmp.path / "append.txt";

    REQUIRE(File::Write(file, "foo").has_value());
    REQUIRE(File::Append(file, "bar").has_value());

    auto result = File::Read(file);
    REQUIRE(result.has_value());
    REQUIRE(*result == "foobar");
}

TEST_CASE("File::Delete removes the file; Exists returns false afterwards", "[unit]")
{
    TempDir tmp;
    Path file = tmp.path / "delete_me.txt";

    REQUIRE(File::Write(file, "x").has_value());
    REQUIRE(File::Exists(file));

    REQUIRE(File::Delete(file).has_value());
    REQUIRE_FALSE(File::Exists(file));
}

TEST_CASE("File::Size returns correct byte count", "[unit]")
{
    TempDir tmp;
    Path file = tmp.path / "sized.txt";
    const std::string content = "12345";

    REQUIRE(File::Write(file, content).has_value());

    auto sz = File::Size(file);
    REQUIRE(sz.has_value());
    REQUIRE(*sz == content.size());
}

TEST_CASE("File::Copy produces a second file with identical content", "[unit]")
{
    TempDir tmp;
    Path src = tmp.path / "src.txt";
    Path dst = tmp.path / "dst.txt";
    const std::string content = "copy me";

    REQUIRE(File::Write(src, content).has_value());
    REQUIRE(File::Copy(src, dst).has_value());

    auto result = File::Read(dst);
    REQUIRE(result.has_value());
    REQUIRE(*result == content);
}

TEST_CASE("File::Move renames the file; original no longer exists", "[unit]")
{
    TempDir tmp;
    Path src = tmp.path / "before.txt";
    Path dst = tmp.path / "after.txt";

    REQUIRE(File::Write(src, "move").has_value());
    REQUIRE(File::Move(src, dst).has_value());

    REQUIRE_FALSE(File::Exists(src));
    REQUIRE(File::Exists(dst));
}
