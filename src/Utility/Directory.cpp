/**
 * @file     Directory.cpp
 * @brief    Directory creation, deletion, listing, and traversal implementation
 *
 * @internal
 * Uses `std::filesystem` throughout. Catches `filesystem_error` and maps
 * to `ImFrame::Error`. No exceptions propagate to callers.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Utility/Directory.hpp"

#include <filesystem>
#include <system_error>

namespace ImFrame::Utility::Directory {

// ─── Create ───────────────────────────────────────────────────────────────────

VoidResult Create(const Path& path)
{
    try {
        std::error_code ec;
        if (!std::filesystem::create_directory(path.Native(), ec) && ec) {
            return std::unexpected(Error::DirectoryCreateFailed);
        }
        return {};
    } catch (...) {
        return std::unexpected(Error::DirectoryCreateFailed);
    }
}

// ─── CreateAll ────────────────────────────────────────────────────────────────

VoidResult CreateAll(const Path& path)
{
    try {
        std::error_code ec;
        std::filesystem::create_directories(path.Native(), ec);
        if (ec) {
            return std::unexpected(Error::DirectoryCreateFailed);
        }
        return {};
    } catch (...) {
        return std::unexpected(Error::DirectoryCreateFailed);
    }
}

// ─── Delete ───────────────────────────────────────────────────────────────────

VoidResult Delete(const Path& path, bool recursive)
{
    try {
        std::error_code ec;
        if (recursive) {
            std::filesystem::remove_all(path.Native(), ec);
        } else {
            std::filesystem::remove(path.Native(), ec);
        }
        if (ec) {
            return std::unexpected(Error::DirectoryDeleteFailed);
        }
        return {};
    } catch (...) {
        return std::unexpected(Error::DirectoryDeleteFailed);
    }
}

// ─── Exists ───────────────────────────────────────────────────────────────────

bool Exists(const Path& path) noexcept
{
    std::error_code ec;
    return std::filesystem::is_directory(path.Native(), ec);
}

// ─── List ─────────────────────────────────────────────────────────────────────

Result<std::vector<Path>> List(const Path& path)
{
    try {
        if (!std::filesystem::exists(path.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        std::vector<Path> entries;
        for (const auto& entry : std::filesystem::directory_iterator{path.Native()}) {
            entries.emplace_back(entry.path());
        }
        return entries;
    } catch (const std::filesystem::filesystem_error&) {
        return std::unexpected(Error::FileReadFailed);
    }
}

// ─── ListRecursive ────────────────────────────────────────────────────────────

Result<std::vector<Path>> ListRecursive(const Path& path)
{
    try {
        if (!std::filesystem::exists(path.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        std::vector<Path> entries;
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator{path.Native()})
        {
            entries.emplace_back(entry.path());
        }
        return entries;
    } catch (const std::filesystem::filesystem_error&) {
        return std::unexpected(Error::FileReadFailed);
    }
}

// ─── Walk (void visitor) ──────────────────────────────────────────────────────

VoidResult Walk(const Path& path, std::function<void(const Path&)> visitor)
{
    try {
        if (!std::filesystem::exists(path.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator{path.Native()})
        {
            visitor(Path{entry.path()});
        }
        return {};
    } catch (const std::filesystem::filesystem_error&) {
        return std::unexpected(Error::FileReadFailed);
    }
}

// ─── Walk (bool visitor — early exit) ────────────────────────────────────────

VoidResult Walk(const Path& path, std::function<bool(const Path&)> visitor)
{
    try {
        if (!std::filesystem::exists(path.Native())) {
            return std::unexpected(Error::FileNotFound);
        }
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator{path.Native()})
        {
            if (!visitor(Path{entry.path()})) {
                break;
            }
        }
        return {};
    } catch (const std::filesystem::filesystem_error&) {
        return std::unexpected(Error::FileReadFailed);
    }
}

} // namespace ImFrame::Utility::Directory
