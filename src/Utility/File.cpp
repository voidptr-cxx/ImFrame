/**
 * @file     File.cpp
 * @brief    Synchronous file I/O implementation
 *
 * @internal
 * All functions catch `std::filesystem::filesystem_error` and map to
 * `ImFrame::Error`. No exceptions propagate to callers.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 */

#include "ImFrame/Utility/File.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace ImFrame::Utility::File {

namespace {

/// Map a `std::filesystem::filesystem_error` to the nearest `Error` code.
Error MapFsError(const std::filesystem::filesystem_error& ex)
{
    const auto code = ex.code();
    if (code == std::errc::no_such_file_or_directory ||
        code == std::errc::not_a_directory)
    {
        return Error::FileNotFound;
    }
    return Error::FileReadFailed;
}

} // anonymous namespace

// ─── Read ─────────────────────────────────────────────────────────────────────

Result<std::string> Read(const Path& path)
{
    try {
        if (!std::filesystem::exists(path.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        // Open in binary mode to avoid CRLF translation on Windows.
        std::ifstream stream{path.Native(), std::ios::binary};
        if (!stream) {
            return std::unexpected(Error::FileReadFailed);
        }
        return std::string{std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{}};
    } catch (const std::filesystem::filesystem_error& ex) {
        return std::unexpected(MapFsError(ex));
    } catch (...) {
        return std::unexpected(Error::FileReadFailed);
    }
}

// ─── ReadBytes ────────────────────────────────────────────────────────────────

Result<std::vector<std::byte>> ReadBytes(const Path& path)
{
    try {
        if (!std::filesystem::exists(path.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        std::ifstream stream{path.Native(), std::ios::binary};
        if (!stream) {
            return std::unexpected(Error::FileReadFailed);
        }
        std::vector<char> chars{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
        std::vector<std::byte> bytes(chars.size());
        std::ranges::transform(chars, bytes.begin(),
            [](char c) { return static_cast<std::byte>(c); });
        return bytes;
    } catch (const std::filesystem::filesystem_error& ex) {
        return std::unexpected(MapFsError(ex));
    } catch (...) {
        return std::unexpected(Error::FileReadFailed);
    }
}

// ─── Write ────────────────────────────────────────────────────────────────────

VoidResult Write(const Path& path, std::string_view content)
{
    try {
        std::ofstream stream{path.Native(),
                             std::ios::binary | std::ios::trunc};
        if (!stream) {
            return std::unexpected(Error::FileWriteFailed);
        }
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!stream) {
            return std::unexpected(Error::FileWriteFailed);
        }
        return {};
    } catch (...) {
        return std::unexpected(Error::FileWriteFailed);
    }
}

// ─── WriteBytes ───────────────────────────────────────────────────────────────

VoidResult WriteBytes(const Path& path, std::span<const std::byte> bytes)
{
    try {
        std::ofstream stream{path.Native(),
                             std::ios::binary | std::ios::trunc};
        if (!stream) {
            return std::unexpected(Error::FileWriteFailed);
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        if (!stream) {
            return std::unexpected(Error::FileWriteFailed);
        }
        return {};
    } catch (...) {
        return std::unexpected(Error::FileWriteFailed);
    }
}

// ─── Append ───────────────────────────────────────────────────────────────────

VoidResult Append(const Path& path, std::string_view content)
{
    try {
        std::ofstream stream{path.Native(),
                             std::ios::binary | std::ios::app};
        if (!stream) {
            return std::unexpected(Error::FileWriteFailed);
        }
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!stream) {
            return std::unexpected(Error::FileWriteFailed);
        }
        return {};
    } catch (...) {
        return std::unexpected(Error::FileWriteFailed);
    }
}

// ─── Exists ───────────────────────────────────────────────────────────────────

bool Exists(const Path& path) noexcept
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path.Native(), ec);
}

// ─── Size ─────────────────────────────────────────────────────────────────────

Result<uint64_t> Size(const Path& path)
{
    try {
        if (!std::filesystem::exists(path.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        return static_cast<uint64_t>(std::filesystem::file_size(path.Native()));
    } catch (const std::filesystem::filesystem_error& ex) {
        return std::unexpected(MapFsError(ex));
    }
}

// ─── LastModified ─────────────────────────────────────────────────────────────

Result<std::filesystem::file_time_type> LastModified(const Path& path)
{
    try {
        if (!std::filesystem::exists(path.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        return std::filesystem::last_write_time(path.Native());
    } catch (const std::filesystem::filesystem_error& ex) {
        return std::unexpected(MapFsError(ex));
    }
}

// ─── Copy ─────────────────────────────────────────────────────────────────────

VoidResult Copy(const Path& src, const Path& dst)
{
    try {
        if (!std::filesystem::exists(src.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        std::filesystem::copy_file(src.Native(), dst.Native(),
            std::filesystem::copy_options::overwrite_existing);
        return {};
    } catch (...) {
        return std::unexpected(Error::FileWriteFailed);
    }
}

// ─── Move ─────────────────────────────────────────────────────────────────────

VoidResult Move(const Path& src, const Path& dst)
{
    try {
        std::filesystem::rename(src.Native(), dst.Native());
        return {};
    } catch (...) {
        return std::unexpected(Error::FileWriteFailed);
    }
}

// ─── Delete ───────────────────────────────────────────────────────────────────

VoidResult Delete(const Path& path)
{
    try {
        std::error_code ec;
        std::filesystem::remove(path.Native(), ec);
        // Silently succeed if the file did not exist.
        if (ec && ec != std::errc::no_such_file_or_directory) {
            return std::unexpected(Error::FileWriteFailed);
        }
        return {};
    } catch (...) {
        return std::unexpected(Error::FileWriteFailed);
    }
}

} // namespace ImFrame::Utility::File
