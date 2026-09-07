/**
 * @file     File.hpp
 * @brief    Synchronous file I/O free functions returning std::expected
 *
 * All functions in `ImFrame::Utility::File` are free functions — a file is a
 * resource addressed by a path, not an object with identity. Every failable
 * operation returns `Result<T>` or `VoidResult`; no function throws.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Utility/Path.hpp"

/**
 * @namespace ImFrame::Utility::File
 * @brief     Synchronous file I/O — read, write, copy, move, delete.
 */
namespace ImFrame::Utility::File {

/**
 * @brief    Reads an entire file as a UTF-8 string.
 *
 * Opens the file in binary mode to avoid CRLF translation on Windows.
 *
 * @param[in]  path  Path to the file to read.
 *
 * @return   The file contents on success, or:
 *           - `Error::FileNotFound`  if `path` does not exist.
 *           - `Error::FileReadFailed` if the file exists but cannot be read.
 *
 * @throws   Nothing.
 */
[[nodiscard]] Result<std::string> Read(const Path& path);

/**
 * @brief    Reads an entire file as raw bytes.
 *
 * @param[in]  path  Path to the file to read.
 *
 * @return   The raw byte content on success, or `Error::FileNotFound` /
 *           `Error::FileReadFailed` on failure.
 *
 * @throws   Nothing.
 */
[[nodiscard]] Result<std::vector<std::byte>> ReadBytes(const Path& path);

/**
 * @brief    Writes text to a file, creating or overwriting it.
 *
 * @param[in]  path     Destination path. Parent directory must exist.
 * @param[in]  content  UTF-8 text to write.
 *
 * @return   Empty result on success, or `Error::FileWriteFailed` on failure.
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult Write(const Path& path, std::string_view content);

/**
 * @brief    Writes raw bytes to a file, creating or overwriting it.
 *
 * @param[in]  path   Destination path. Parent directory must exist.
 * @param[in]  bytes  Bytes to write.
 *
 * @return   Empty result on success, or `Error::FileWriteFailed` on failure.
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult WriteBytes(const Path& path, std::span<const std::byte> bytes);

/**
 * @brief    Appends text to a file, creating it if it does not exist.
 *
 * @param[in]  path     Target file path.
 * @param[in]  content  UTF-8 text to append.
 *
 * @return   Empty result on success, or `Error::FileWriteFailed` on failure.
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult Append(const Path& path, std::string_view content);

/**
 * @brief    Returns `true` if the path names an existing regular file.
 *
 * @note     Does not distinguish between missing and inaccessible; both
 *           return `false`. Use `File::Read` if you need the distinction.
 *
 * @return   `true` if a regular file exists at `path`.
 */
[[nodiscard]] bool Exists(const Path& path) noexcept;

/**
 * @brief    Returns the size of the file in bytes.
 *
 * @return   File size on success, or `Error::FileNotFound` if missing.
 *
 * @throws   Nothing.
 */
[[nodiscard]] Result<uint64_t> Size(const Path& path);

/**
 * @brief    Returns the last-write time of the file.
 *
 * @return   A `std::filesystem::file_time_type` on success, or
 *           `Error::FileNotFound` if the file does not exist.
 *
 * @throws   Nothing.
 */
[[nodiscard]] Result<std::filesystem::file_time_type> LastModified(const Path& path);

/**
 * @brief    Copies a file from `src` to `dst`, overwriting if `dst` exists.
 *
 * @param[in]  src  Source file path.
 * @param[in]  dst  Destination file path.
 *
 * @return   Empty result on success, or `Error::FileNotFound` /
 *           `Error::FileWriteFailed` on failure.
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult Copy(const Path& src, const Path& dst);

/**
 * @brief    Moves (renames) `src` to `dst`.
 *
 * @param[in]  src  Source file path.
 * @param[in]  dst  Destination file path.
 *
 * @return   Empty result on success, or `Error::FileWriteFailed` on failure.
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult Move(const Path& src, const Path& dst);

/**
 * @brief    Deletes the file at `path`.
 *
 * Silently succeeds if the file does not exist.
 *
 * @return   Empty result on success, or `Error::FileWriteFailed` if deletion
 *           fails for a reason other than non-existence.
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult Delete(const Path& path);

} // namespace ImFrame::Utility::File
