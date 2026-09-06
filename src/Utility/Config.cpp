/**
 * @file     Config.cpp
 * @brief    Config implementation — TOML-subset parser, Get/Set/Save, file watching
 *
 * @internal
 * The TOML subset parser handles:
 *   - `[section]` headers: sets the current section prefix
 *   - `key = value` pairs: value is parsed as bool, int64, double, or quoted string
 *   - `# comment` lines: skipped
 *   - Blank lines: skipped
 *
 * Values are stored as `ConfigValue` (std::variant) in an unordered_map keyed
 * by "section.key" dot notation. Top-level keys (no section) use just "key".
 *
 * Background save captures Impl* (raw pointer) not Config*. This is safe
 * because BackgroundWorker::Drain() is called in Config's destructor before
 * the unique_ptr<Impl> is destroyed — the Impl outlives all posted tasks.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Utility/Config.hpp"
#include "ImFrame/Utility/BackgroundWorker.hpp"
#include "ImFrame/Utility/FileWatcher.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <shared_mutex>
#include <sstream>

namespace ImFrame::Utility {

// ─── TOML-subset parser (file-local) ─────────────────────────────────────────

namespace {

std::string Trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool TryParseValue(const std::string& token, ConfigValue& out) {
    if (token == "true")  { out = true;  return true; }
    if (token == "false") { out = false; return true; }

    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        out = token.substr(1, token.size() - 2);
        return true;
    }

    try {
        std::size_t pos = 0;
        int64_t iv = std::stoll(token, &pos);
        if (pos == token.size()) { out = iv; return true; }
    } catch (...) {}

    try {
        std::size_t pos = 0;
        double dv = std::stod(token, &pos);
        if (pos == token.size()) { out = dv; return true; }
    } catch (...) {}

    return false;
}

// Returns empty string on success, or an error description on failure.
std::string ParseToml(const std::string& text,
                      std::unordered_map<std::string, ConfigValue>& out) {
    std::istringstream ss{text};
    std::string line;
    std::string section;

    while (std::getline(ss, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        if (trimmed[0] == '[') {
            const auto close = trimmed.find(']');
            if (close == std::string::npos) return "missing ']' in section header";
            section = Trim(trimmed.substr(1, close - 1));
            continue;
        }

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) return "missing '=' in: " + trimmed;

        std::string key   = Trim(trimmed.substr(0, eq));
        std::string value = Trim(trimmed.substr(eq + 1));

        // Strip inline comments outside quoted strings
        bool inQuote = false;
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '"') inQuote = !inQuote;
            if (!inQuote && value[i] == '#') {
                value = Trim(value.substr(0, i));
                break;
            }
        }

        if (key.empty()) return "empty key in: " + trimmed;

        std::string fullKey = section.empty() ? key : (section + '.' + key);
        ConfigValue cv;
        if (!TryParseValue(value, cv)) return "cannot parse value: " + value;
        out[fullKey] = std::move(cv);
    }
    return {};
}

std::string ValueToToml(const ConfigValue& v) {
    // MSVC C4702: all variant alternatives in the if constexpr chain are
    // exhaustive but MSVC still warns about the dead fallthrough return.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4702)
#endif
    return std::visit([](const auto& val) -> std::string {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, bool>)        return val ? "true" : "false";
        if constexpr (std::is_same_v<T, int64_t>)     return std::to_string(val);
        if constexpr (std::is_same_v<T, double>)      return std::to_string(val);
        if constexpr (std::is_same_v<T, std::string>) return '"' + val + '"';
        return {};
    }, v);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
}

std::string SerialiseToml(const std::unordered_map<std::string, ConfigValue>& data) {
    // Group by section for deterministic output
    std::map<std::string, std::map<std::string, const ConfigValue*>> sections;
    for (auto& [k, v] : data) {
        const auto dot = k.find('.');
        if (dot == std::string::npos) sections[""][k] = &v;
        else sections[k.substr(0, dot)][k.substr(dot + 1)] = &v;
    }

    std::string out;
    for (auto& [key, valPtr] : sections[""]) {
        out += key + " = " + ValueToToml(*valPtr) + "\n";
    }
    sections.erase("");

    for (auto& [section, keys] : sections) {
        out += "\n[" + section + "]\n";
        for (auto& [key, valPtr] : keys) {
            out += key + " = " + ValueToToml(*valPtr) + "\n";
        }
    }
    return out;
}

} // namespace

// ─── Config::Impl ─────────────────────────────────────────────────────────────

struct Config::Impl {
    mutable std::shared_mutex                    mutex;
    std::unordered_map<std::string, ConfigValue> data;
    Path                                         savePath;
    BackgroundWorker                             saveWorker{"Config-Save"};
    std::atomic<bool>                            saveQueued{false};
    FileWatcher                                  watcher;
    WatchHandle                                  watchHandle{InvalidWatchHandle};
    Path                                         watchedPath;
    Signal<void()>                               onChanged;
};


// ─── Config lifecycle ─────────────────────────────────────────────────────────

Config::Config()
    : _impl{std::make_unique<Impl>()} {}

Config::~Config() noexcept {
    if (_impl) {
        if (_impl->watchHandle != InvalidWatchHandle) {
            _impl->watcher.Unwatch(_impl->watchHandle);
        }
        _impl->saveWorker.Drain();
    }
}

Config::Config(Config&&) noexcept = default;

Config& Config::operator=(Config&& other) noexcept {
    if (this != &other) {
        if (_impl) {
            if (_impl->watchHandle != InvalidWatchHandle) {
                _impl->watcher.Unwatch(_impl->watchHandle);
            }
            _impl->saveWorker.Drain();
        }
        _impl = std::move(other._impl);
    }
    return *this;
}

// ─── Config::Load ─────────────────────────────────────────────────────────────

ImFrame::Result<Config> Config::Load(const Path& path) {
    if (!std::filesystem::exists(path.Native()))
        return std::unexpected(ImFrame::Error::FileNotFound);

    std::ifstream file{path.Native()};
    if (!file.is_open())
        return std::unexpected(ImFrame::Error::FileReadFailed);

    std::string text{std::istreambuf_iterator<char>{file},
                     std::istreambuf_iterator<char>{}};
    if (file.fail() && !file.eof())
        return std::unexpected(ImFrame::Error::FileReadFailed);

    Config cfg;
    if (auto err = ParseToml(text, cfg._impl->data); !err.empty())
        return std::unexpected(ImFrame::Error::ConfigParseFailed);

    cfg._impl->savePath = path;
    return cfg;
}

// ─── Config::Get / Set helpers ────────────────────────────────────────────────

const ConfigValue* Config::GetValuePtr(const std::string& key) const {
    std::shared_lock lock{_impl->mutex};
    auto it = _impl->data.find(key);
    return (it != _impl->data.end()) ? &it->second : nullptr;
}

void Config::SetValue(const std::string& key, ConfigValue value) {
    {
        std::unique_lock lock{_impl->mutex};
        _impl->data[key] = std::move(value);
    }
    // Coalesced save: capturing 'this' is safe — Drain() in the destructor and
    // move-assignment ensures all tasks complete before 'this' is invalidated.
    bool expected = false;
    if (_impl->saveQueued.compare_exchange_strong(expected, true,
                                                   std::memory_order_acq_rel)) {
        _impl->saveWorker.Post([this]() {
            if (!_impl->savePath.IsEmpty()) (void)Save(_impl->savePath);
            _impl->saveQueued.store(false, std::memory_order_release);
        });
    }
}

// ─── Config::Save ─────────────────────────────────────────────────────────────

ImFrame::VoidResult Config::Save(const Path& path) {
    // Exclusive (not shared) and held across the actual file write, not just the data->string
    // serialisation: SetValue()'s coalesced background save and a caller's own explicit Save()
    // both end up here, on different threads, targeting the same path. A shared_lock released
    // before the ofstream write let two such calls interleave their writes to the same file --
    // e.g. one truncates just as the other is mid-write -- corrupting it. Since SetValue() always
    // applies each Set() to _impl->data synchronously (under its own unique_lock) before this ever
    // runs, whichever of two racing Save() calls a caller-vs-worker pair happens to run first, both
    // would serialise the identical, fully up-to-date content anyway -- so full serialisation here
    // costs nothing but a Save() call rarely overlapping with an unrelated Get()/Set() on the same
    // instance, and fixes the real, reproducible file corruption. See .claude/DECISIONS.md.
    std::unique_lock lock{_impl->mutex};
    const std::string text = SerialiseToml(_impl->data);

    std::ofstream out{path.Native(), std::ios::trunc};
    if (!out.is_open()) return std::unexpected(ImFrame::Error::FileWriteFailed);
    out << text;
    if (!out) return std::unexpected(ImFrame::Error::FileWriteFailed);
    _impl->savePath = path;
    return {};
}

// ─── Config::WatchPath / PollWatcher ──────────────────────────────────────────

void Config::WatchPath(const Path& path) {
    if (_impl->watchHandle != InvalidWatchHandle) {
        _impl->watcher.Unwatch(_impl->watchHandle);
        _impl->watchHandle = InvalidWatchHandle;
    }
    _impl->watchedPath = path;
    if (auto result = _impl->watcher.Watch(path.Parent()); result.has_value()) {
        _impl->watchHandle = *result;
    }
}

void Config::PollWatcher() {
    if (_impl->watchHandle == InvalidWatchHandle) return;

    (void)_impl->watcher.Poll([this](const FileEvent& e) {
        if (e.Type != FileChangeType::Modified) return;
        if (e.ChangedPath.Native().filename() != _impl->watchedPath.Native().filename()) return;

        std::ifstream file{_impl->watchedPath.Native()};
        if (!file.is_open()) return;

        std::string text{std::istreambuf_iterator<char>{file},
                         std::istreambuf_iterator<char>{}};
        if (file.fail() && !file.eof()) return;

        std::unordered_map<std::string, ConfigValue> newData;
        if (!ParseToml(text, newData).empty()) return;

        {
            std::unique_lock lock{_impl->mutex};
            _impl->data = std::move(newData);
        }
        _impl->onChanged.Emit();
    });
}

// ─── Config::OnChanged ────────────────────────────────────────────────────────

Signal<void()>& Config::OnChanged() noexcept {
    return _impl->onChanged;
}

} // namespace ImFrame::Utility
