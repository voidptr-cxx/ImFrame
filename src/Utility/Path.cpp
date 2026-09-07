/**
 * @file     Path.cpp
 * @brief    Path implementation — platform-specific named constructors
 *
 * @internal
 * Platform guards:
 *   _WIN32   → GetModuleFileNameW, SHGetKnownFolderPath
 *   __APPLE__ → _NSGetExecutablePath
 *   (else)   → /proc/self/exe, getenv("HOME")
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Utility/Path.hpp"

#include <cstdlib>      // getenv
#include <filesystem>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <shlobj.h>     // SHGetKnownFolderPath, FOLDERID_Profile
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>    // _NSGetExecutablePath
    #include <limits.h>
#else
    #include <unistd.h>         // readlink
    #include <limits.h>         // PATH_MAX
#endif

namespace ImFrame::Utility {

// ─── Named constructors ───────────────────────────────────────────────────────

Path Path::FromCwd() noexcept
{
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    if (ec) return {};
    return Path{std::move(p)};
}

Path Path::FromExecutable() noexcept
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH) == 0) return {};
    return Path{std::filesystem::path{buf}.parent_path()};

#elif defined(__APPLE__)
    char buf[PATH_MAX] = {};
    uint32_t size = PATH_MAX;
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    std::error_code ec;
    auto resolved = std::filesystem::canonical(buf, ec);
    if (ec) return {};
    return Path{resolved.parent_path()};

#else // Linux
    char buf[PATH_MAX] = {};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return {};
    return Path{std::filesystem::path{buf}.parent_path()};
#endif
}

Path Path::FromUserHome() noexcept
{
#ifdef _WIN32
    PWSTR pszPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &pszPath))) {
        return {};
    }
    Path result{std::filesystem::path{pszPath}};
    CoTaskMemFree(pszPath);
    return result;
#else
    const char* home = std::getenv("HOME");
    if (!home) return {};
    return Path{home};
#endif
}

// ─── Construction ─────────────────────────────────────────────────────────────

Path::Path(std::string_view str) noexcept
    : _path(std::filesystem::path{str})
{}

Path::Path(const char* str) noexcept
    : _path(std::filesystem::path{str})
{}

Path::Path(std::filesystem::path p) noexcept
    : _path(std::move(p))
{}

// ─── Chainable accessors ──────────────────────────────────────────────────────

Path Path::Parent() const noexcept { return Path{_path.parent_path()}; }
Path Path::Stem()   const noexcept { return Path{_path.stem()};        }

Path Path::Extension() const noexcept { return Path{_path.extension()}; }

Path Path::WithExtension(std::string_view ext) const noexcept
{
    auto p = _path;
    p.replace_extension(std::filesystem::path{ext});
    return Path{std::move(p)};
}

Path Path::Append(std::string_view segment) const noexcept
{
    return Path{_path / std::filesystem::path{segment}};
}

Path Path::operator/(std::string_view segment) const noexcept
{
    return Append(segment);
}

// ─── Conversion ───────────────────────────────────────────────────────────────

std::string Path::ToString() const
{
    return _path.string();
}

std::u8string Path::ToU8String() const
{
    return _path.u8string();
}

const std::filesystem::path& Path::Native() const noexcept
{
    return _path;
}

// ─── Queries ──────────────────────────────────────────────────────────────────

bool Path::IsEmpty()    const noexcept { return _path.empty(); }
bool Path::IsAbsolute() const noexcept { return _path.is_absolute(); }
bool Path::IsRelative() const noexcept { return _path.is_relative(); }

} // namespace ImFrame::Utility
