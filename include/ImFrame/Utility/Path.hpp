/**
 * @file     Path.hpp
 * @brief    Immutable filesystem path wrapper with ergonomic named constructors
 *
 * `Path` wraps `std::filesystem::path` with chainable accessors and platform-
 * aware named constructors for the current directory, the executable location,
 * and the user's home directory. All operations are `[[nodiscard]]` and
 * `noexcept`. `Path` has value semantics and is trivially copyable.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace ImFrame::Utility {

/**
 * @class    Path
 * @brief    Immutable filesystem path with ergonomic named constructors and chainable accessors
 *
 * `Path` wraps `std::filesystem::path` and adds:
 * - Platform-specific named constructors (`FromCwd`, `FromExecutable`, `FromUserHome`)
 * - Chainable mutation methods — each returns a new `Path`, the original is unchanged
 * - Implicit construction from `std::string_view` and `const char*` for ergonomic use
 *
 * All methods are `[[nodiscard]]` and `noexcept`. `Path` has value semantics.
 *
 * @since    0.3.0
 *
 * @example
 * @code
 * using ImFrame::Utility::Path;
 * auto config = Path::FromUserHome() / ".config" / "myapp" / "settings.toml";
 * std::string str = config.ToString();
 * @endcode
 */
class Path {
public:
    // ─── Named constructors ───────────────────────────────────────────────────

    /**
     * @brief    Returns the current working directory.
     * @return   The process CWD as a Path. Returns an empty Path on error.
     */
    [[nodiscard]] static Path FromCwd() noexcept;

    /**
     * @brief    Returns the directory containing the running executable.
     *
     * Platform-specific: `_NSGetExecutablePath` on macOS, `/proc/self/exe`
     * on Linux, `GetModuleFileNameW` on Windows. Returns an empty Path if the
     * platform lookup fails.
     */
    [[nodiscard]] static Path FromExecutable() noexcept;

    /**
     * @brief    Returns the current user's home directory.
     *
     * Uses `$HOME` on POSIX systems and `SHGetKnownFolderPath(FOLDERID_Profile)`
     * on Windows. Returns an empty Path if the environment variable is unset.
     */
    [[nodiscard]] static Path FromUserHome() noexcept;

    // ─── Construction ─────────────────────────────────────────────────────────

    /// @brief  Default constructor. Constructs an empty path.
    Path() noexcept = default;

    /**
     * @brief   Implicit construction from a string view.
     * @param[in]  str  UTF-8 path string.
     */
    Path(std::string_view str) noexcept; // NOLINT(google-explicit-constructor)

    /**
     * @brief   Implicit construction from a C-string literal.
     * @param[in]  str  Null-terminated UTF-8 path.
     */
    Path(const char* str) noexcept; // NOLINT(google-explicit-constructor)

    /**
     * @brief   Explicit construction from a native `std::filesystem::path`.
     * @param[in]  p  The underlying path value.
     */
    explicit Path(std::filesystem::path p) noexcept;

    // ─── Chainable accessors ──────────────────────────────────────────────────

    /// @return  The parent directory of this path.
    [[nodiscard]] Path Parent()    const noexcept;

    /// @return  The filename without extension.
    [[nodiscard]] Path Stem()      const noexcept;

    /// @return  The file extension including the leading dot (e.g. `".hpp"`).
    [[nodiscard]] Path Extension() const noexcept;

    /**
     * @brief    Returns a copy of this path with the extension replaced.
     * @param[in]  ext  New extension, with or without a leading dot.
     */
    [[nodiscard]] Path WithExtension(std::string_view ext) const noexcept;

    /**
     * @brief    Appends a path segment and returns the result.
     * @param[in]  segment  Segment to append. May contain separators.
     */
    [[nodiscard]] Path Append(std::string_view segment) const noexcept;

    /**
     * @brief    Alias for Append().
     * @param[in]  segment  Segment to append.
     */
    [[nodiscard]] Path operator/(std::string_view segment) const noexcept;

    // ─── Conversion ───────────────────────────────────────────────────────────

    /// @return  The path as a UTF-8 `std::string`.
    [[nodiscard]] std::string ToString() const;

    /// @return  The path as a `std::u8string`.
    [[nodiscard]] std::u8string ToU8String() const;

    /// @return  The underlying `std::filesystem::path` (read-only).
    [[nodiscard]] const std::filesystem::path& Native() const noexcept;

    // ─── Queries ──────────────────────────────────────────────────────────────

    /// @return  `true` if the path is empty (default-constructed or cleared).
    [[nodiscard]] bool IsEmpty()    const noexcept;

    /// @return  `true` if the path is absolute.
    [[nodiscard]] bool IsAbsolute() const noexcept;

    /// @return  `true` if the path is relative.
    [[nodiscard]] bool IsRelative() const noexcept;

    // ─── Comparison ───────────────────────────────────────────────────────────

    bool operator==(const Path&) const noexcept = default;

private:
    std::filesystem::path _path;
};

} // namespace ImFrame::Utility
