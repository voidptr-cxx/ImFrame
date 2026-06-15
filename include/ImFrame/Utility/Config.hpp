/**
 * @file     Config.hpp
 * @brief    Persistent key-value configuration store with TOML-subset parser
 *
 * `Config` provides typed get/set access to configuration values backed by a
 * hand-written TOML-subset file format. Only the constructs needed for flat
 * sections and primitive values are supported (see below).
 *
 * **Supported TOML constructs:**
 *  - `[section]` headers — nested keys use dot notation: `"section.key"`
 *  - `key = value` assignments at the top level or inside a section
 *  - Value types: boolean (`true`/`false`), integer, float, quoted string
 *  - Line comments: `# ...`
 *
 * **Usage pattern:**
 * ```
 * [Window]
 * Width  = 1280
 * Height = 720
 * Title  = "My App"
 * ```
 * Accessed as `config.Get<int64_t>("Window.Width", 800)`.
 *
 * `Set()` updates a value, marks the config dirty, and enqueues a coalesced
 * deferred save so that at most one save task is queued at any time.
 *
 * `WatchPath()` registers a `FileWatcher` subscription; call `PollWatcher()`
 * once per frame to dispatch file-change events and trigger auto-reload.
 *
 * Supported types for `Get<T>` / `Set<T>`:
 *   `bool`, `int64_t`, `double`, `std::string`
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Core/Error.hpp"
#include "ImFrame/Utility/Path.hpp"
#include "ImFrame/Utility/Signal.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

namespace ImFrame::Utility {

/// @brief  Variant holding any value supported by Config::Get / Config::Set
using ConfigValue = std::variant<bool, int64_t, double, std::string>;

/**
 * @class    Config
 * @brief    Typed key-value store backed by a TOML-subset file
 *
 * Config is move-only (the Pimpl pointer transfers on move). Use the static
 * factory `Config::Load()` to create an instance from a file. The default
 * constructor creates an empty in-memory config.
 *
 * Thread safety: `Get()` and `Set()` are safe to call concurrently from
 * multiple threads. `PollWatcher()` must be called from the owner thread.
 *
 * @since    0.6.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 *
 * auto result = Config::Load(Path::FromUserHome() / ".config/myapp/settings.toml");
 * if (!result) { /* handle error *\/ }
 * Config config = std::move(*result);
 *
 * auto width = config.Get<int64_t>("Window.Width", 1280);
 * config.Set("Window.Width", int64_t{1920});
 *
 * auto conn = config.OnChanged().Connect([]() { reloadUI(); });
 *
 * // In the frame loop:
 * config.PollWatcher();
 * @endcode
 */
class Config {
public:
    /**
     * @brief  Constructs an empty in-memory Config with no backing file
     */
    Config();

    /**
     * @brief  Destructor — drains any pending background saves before destruction
     */
    ~Config() noexcept;

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    /**
     * @brief  Move constructor — transfers the Pimpl pointer
     */
    Config(Config&&) noexcept;

    /**
     * @brief  Move-assignment — drains current saves, then transfers Pimpl
     */
    Config& operator=(Config&&) noexcept;

    // ── Factory ───────────────────────────────────────────────────────────

    /**
     * @brief    Reads and parses a TOML-subset configuration file synchronously
     *
     * @param[in] path  Path to the TOML configuration file.
     *
     * @return   A fully-initialised Config on success, or:
     *           - `Error::FileNotFound`      — path does not exist
     *           - `Error::FileReadFailed`    — file cannot be read
     *           - `Error::ConfigParseFailed` — TOML syntax error
     */
    [[nodiscard]] static ImFrame::Result<Config> Load(const Path& path);

    // ── Access ────────────────────────────────────────────────────────────

    /**
     * @brief    Returns the stored value for `key`, or `defaultValue` if absent
     *
     * Returns `defaultValue` when the key is absent or the stored type does not
     * match `T`. Valid types: `bool`, `int64_t`, `double`, `std::string`.
     *
     * @tparam   T            One of `bool`, `int64_t`, `double`, `std::string`.
     * @param[in] key          Key string, dot-notation for sectioned keys.
     * @param[in] defaultValue Returned when the key is absent or type-mismatched.
     *
     * @return   The stored value or `defaultValue`.
     */
    template <typename T>
    [[nodiscard]] T Get(std::string_view key, T defaultValue) const {
        const ConfigValue* val = GetValuePtr(std::string(key));
        if (!val) return defaultValue;
        const T* typed = std::get_if<T>(val);
        return typed ? *typed : defaultValue;
    }

    /**
     * @brief    Stores `value` for `key` and schedules a coalesced deferred save
     *
     * At most one save task is queued at any time — rapid successive `Set()`
     * calls coalesce into a single save that captures all of them.
     *
     * @tparam   T      One of `bool`, `int64_t`, `double`, `std::string`.
     * @param[in] key    Key string, dot-notation for sectioned keys.
     * @param[in] value  The value to store.
     */
    template <typename T>
    void Set(std::string_view key, T value) {
        SetValue(std::string(key), ConfigValue{std::move(value)});
    }

    // ── Persistence ───────────────────────────────────────────────────────

    /**
     * @brief    Saves the current state synchronously to `path`
     *
     * Updates the internal save-path used by the coalesced background saver.
     *
     * @param[in] path  Destination file path.
     *
     * @return   `std::unexpected(Error::FileWriteFailed)` if write fails.
     */
    [[nodiscard]] ImFrame::VoidResult Save(const Path& path);

    // ── File watching ─────────────────────────────────────────────────────

    /**
     * @brief    Registers a FileWatcher subscription on the config file's directory
     *
     * @param[in] path  Path to the config FILE to monitor (not a directory).
     */
    void WatchPath(const Path& path);

    /**
     * @brief  Dispatches pending FileWatcher events; call once per frame
     *
     * Reloads config from disk if the watched file was modified, then fires
     * `OnChanged`. Must be called from the owner thread.
     */
    void PollWatcher();

    // ── Signal ────────────────────────────────────────────────────────────

    /**
     * @brief   Returns the OnChanged signal for subscribing to reload events
     *
     * Fires on the owner thread during `PollWatcher()` when the config file
     * is modified externally and successfully reloaded.
     *
     * @return  Reference to the signal (lifetime tied to this Config).
     */
    [[nodiscard]] Signal<void()>& OnChanged() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

    // Non-template helpers — implemented in Config.cpp where Impl is fully visible
    [[nodiscard]] const ConfigValue* GetValuePtr(const std::string& key) const;
    void SetValue(const std::string& key, ConfigValue value);
};

} // namespace ImFrame::Utility
