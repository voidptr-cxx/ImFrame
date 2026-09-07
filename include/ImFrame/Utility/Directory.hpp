/**
 * @file     Directory.hpp
 * @brief    Directory creation, deletion, listing, and traversal free functions
 *
 * `ImFrame::Utility::Directory` mirrors `ImFrame::Utility::File`'s design —
 * all operations are free functions, none throw, and all failable operations
 * return `Result<T>` or `VoidResult`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2025-01-15
 * @version  0.3.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <functional>
#include <vector>

#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Utility/Path.hpp"

/**
 * @namespace ImFrame::Utility::Directory
 * @brief     Directory management — create, delete, list, and walk.
 */
namespace ImFrame::Utility::Directory {

/**
 * @brief    Creates a single directory at `path`.
 *
 * All parent directories must already exist. Use `CreateAll` to create
 * the entire path hierarchy in one call.
 *
 * @param[in]  path  Directory to create.
 *
 * @return   Empty result on success, or `Error::DirectoryCreateFailed` on
 *           failure (e.g. parent does not exist, permission denied).
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult Create(const Path& path);

/**
 * @brief    Creates `path` and all missing parent directories (`mkdir -p`).
 *
 * Equivalent to `std::filesystem::create_directories`. Silently succeeds
 * if the directory already exists.
 *
 * @param[in]  path  Directory path to create (including all parents).
 *
 * @return   Empty result on success, or `Error::DirectoryCreateFailed` on
 *           failure.
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult CreateAll(const Path& path);

/**
 * @brief    Deletes the directory at `path`.
 *
 * @param[in]  path       Directory to delete.
 * @param[in]  recursive  If `true`, deletes all contents recursively.
 *                        If `false` (default), fails if the directory is
 *                        not empty.
 *
 * @return   Empty result on success, or `Error::DirectoryDeleteFailed` on
 *           failure.
 *
 * @throws   Nothing.
 */
[[nodiscard]] VoidResult Delete(const Path& path, bool recursive = false);

/**
 * @brief    Returns `true` if a directory exists at `path`.
 *
 * @return   `true` if `path` names an existing directory.
 */
[[nodiscard]] bool Exists(const Path& path) noexcept;

/**
 * @brief    Returns the immediate children of `path` (non-recursive).
 *
 * Includes both files and sub-directories. Order is unspecified.
 *
 * @param[in]  path  Directory to list.
 *
 * @return   Vector of child paths on success, or `Error::FileNotFound` if
 *           `path` does not exist, or `Error::FileReadFailed` on I/O error.
 *
 * @throws   Nothing.
 */
[[nodiscard]] Result<std::vector<Path>> List(const Path& path);

/**
 * @brief    Returns all descendants of `path` recursively.
 *
 * The entire directory tree is materialised into the returned vector.
 * For large trees, prefer `Walk`.
 *
 * @param[in]  path  Root directory.
 *
 * @return   Vector of all descendant paths on success.
 *
 * @throws   Nothing.
 */
[[nodiscard]] Result<std::vector<Path>> ListRecursive(const Path& path);

/**
 * @brief    Lazily traverses all descendants of `path`, calling `visitor`
 *           for each entry.
 *
 * Does not materialise the full list — entries are visited as they are
 * discovered. Prefer `Walk` over `ListRecursive` for large directory trees.
 *
 * This overload always continues to the next entry.
 *
 * @param[in]  path     Root directory to traverse.
 * @param[in]  visitor  Called for each entry; `void(const Path&)`.
 *
 * @return   Empty result on success, or `Error::FileNotFound` /
 *           `Error::FileReadFailed` on access error.
 *
 * @throws   Nothing (exceptions from `visitor` propagate unchanged).
 *
 * @see      Walk(const Path&, std::function<bool(const Path&)>) for
 *           early-exit traversal.
 */
[[nodiscard]] VoidResult Walk(const Path& path,
                              std::function<void(const Path&)> visitor);

/**
 * @brief    Lazily traverses all descendants, stopping early when `visitor`
 *           returns `false`.
 *
 * @param[in]  path     Root directory to traverse.
 * @param[in]  visitor  Called for each entry. Return `false` to stop.
 *
 * @return   Empty result on success (including early exit), or
 *           `Error::FileNotFound` / `Error::FileReadFailed` on access error.
 *
 * @throws   Nothing (exceptions from `visitor` propagate unchanged).
 */
[[nodiscard]] VoidResult Walk(const Path& path,
                              std::function<bool(const Path&)> visitor);

} // namespace ImFrame::Utility::Directory
