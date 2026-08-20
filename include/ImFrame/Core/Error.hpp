/**
 * @file     Error.hpp
 * @brief    Error codes, result aliases, and debug assertion macro for ImFrame
 *
 * All fallible public API functions return `ImFrame::VoidResult` or
 * `ImFrame::Result<T>` rather than throwing exceptions. This header provides
 * the `Error` enum, the `std::expected`-based aliases, and the `IMF_ASSERT`
 * macro for internal invariant checking.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.2.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <cassert>
#include <expected>

namespace ImFrame::Core {

/**
 * @enum     Error
 * @brief    Recoverable error conditions that ImFrame operations can return
 *
 * Returned via `std::expected<T, Error>` — never thrown. Callers must
 * inspect the result and handle errors explicitly.
 *
 * @since    0.2.0
 */
enum class Error {
    /// OS window or surface could not be created.
    WindowCreationFailed,

    /// Graphics API (OpenGL, Vulkan, etc.) could not be initialised.
    GraphicsInitFailed,

    /// The required OpenGL version is not supported by this GPU/driver.
    UnsupportedGLVersion,

    /// A caller-supplied argument was invalid (e.g. zero width or height).
    InvalidArgument,

    /// An operation was called before Init() completed successfully.
    NotInitialised,

    /// Init() was called on a backend that is already initialised.
    AlreadyInitialised,

    /// A file path does not exist or is not readable.
    FileNotFound,

    /// A file exists but could not be fully read.
    FileReadFailed,

    /// A file could not be written.
    FileWriteFailed,

    /// A directory could not be created (permissions, invalid path, etc.).
    DirectoryCreateFailed,

    /// A directory could not be deleted.
    DirectoryDeleteFailed,

    /// FileWatcher could not begin monitoring a path.
    WatchFailed,

    /// A configuration file could not be parsed (invalid TOML syntax or unsupported construct).
    ConfigParseFailed,

    /// The requested operation is not available on this backend/platform
    /// (e.g. multi-window or FileWatcher on the Emscripten backend, Phase 23).
    NotSupported,

    /// A font file exists but FreeType/HarfBuzz could not load it (corrupt data,
    /// unsupported format, or library initialisation failure) — Phase 33.
    FontLoadFailed,
};

} // namespace ImFrame::Core

namespace ImFrame {

// ─── Re-export to root namespace ──────────────────────────────────────────────
// Both ImFrame::Core::Error and ImFrame::Error are valid.
/// @copydoc ImFrame::Core::Error
using Error = Core::Error;

/**
 * @brief    Success-or-error result carrying a value of type T on success
 *
 * @tparam   T  The success value type.
 *
 * @since    0.2.0
 *
 * @example
 * @code
 * ImFrame::Result<int> SafeDivide(int a, int b) {
 *     if (b == 0) return std::unexpected(ImFrame::Error::InvalidArgument);
 *     return a / b;
 * }
 * @endcode
 */
template<typename T>
using Result = std::expected<T, Error>;

/**
 * @brief    Success-or-error result for operations that return no value
 *
 * Use `std::unexpected(ImFrame::Error::X)` to construct the error case.
 *
 * @since    0.2.0
 */
using VoidResult = std::expected<void, Error>;

} // namespace ImFrame

// ─── IMF_ASSERT ───────────────────────────────────────────────────────────────

/**
 * @def      IMF_ASSERT(condition)
 * @brief    Debug-mode assertion for **internal** invariants only
 *
 * In debug builds (`NDEBUG` not defined) evaluates `condition` and triggers
 * `assert()` if it is false, printing a diagnostic and aborting.
 * In release builds (`NDEBUG` defined) `condition` is **not evaluated**.
 *
 * @param    condition  Boolean expression. Must have no side-effects —
 *                      it is elided in release builds.
 *
 * @warning  Do **not** use IMF_ASSERT to validate caller-supplied arguments
 *           or expected failure conditions (missing file, unsupported GPU, etc.).
 *           Use `ImFrame::VoidResult` / `ImFrame::Error` for those.
 *           IMF_ASSERT is exclusively for internal programming-error detection.
 */
#ifdef NDEBUG
    #define IMF_ASSERT(condition) ((void)0)
#else
    #define IMF_ASSERT(condition) assert(condition)
#endif
