/**
 * @file     ThemeHotReload.cpp
 * @brief    ThemeHotReload — FileWatcher-backed live theme loading from TOML files
 *
 * @internal
 * The TOML parser here is a minimal specialisation for theme files only. It
 * recognises `[colors]` section headers and float-array values of the form
 * `key = [r, g, b, a]`. Other value types and sections are silently ignored.
 *
 * Key names match the Theme struct field names (camelCase). Unknown keys are
 * silently skipped so that partial overrides work correctly.
 *
 * On Windows the FileWatcher uses `ReadDirectoryChangesW`. The watcher
 * background thread may batch events; a single save triggers one or two
 * Modified events. Both are handled identically — the file is read fresh each
 * time.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-13
 * @version  1.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#if defined(IMF_DEV_TOOLS)

#include "ImFrame/DevTools/ThemeHotReload.hpp"
#include "ImFrame/Theme/ThemeBuilder.hpp"
#include "ImFrame/Utility/File.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace ImFrame::DevTools {

namespace {

// ─── TOML mini-parser (theme files only) ────────────────────────────────────

std::string Trim(std::string_view sv)
{
    const auto s = sv.find_first_not_of(" \t\r\n");
    if (s == std::string_view::npos) return {};
    const auto e = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(s, e - s + 1));
}

// Parses "[r, g, b, a]" into a ColorToken; returns nullopt on failure.
std::optional<Theme::ColorToken> ParseRgba(const std::string& token)
{
    const auto lb = token.find('[');
    const auto rb = token.find(']');
    if (lb == std::string::npos || rb == std::string::npos || rb <= lb) return std::nullopt;

    const std::string inner = token.substr(lb + 1, rb - lb - 1);
    float components[4]{};
    int   idx = 0;

    std::istringstream ss{inner};
    std::string part;
    while (std::getline(ss, part, ',') && idx < 4) {
        const std::string trimmed = Trim(part);
        float v = 0.0f;
        auto [ptr, ec] = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), v);
        if (ec != std::errc{}) return std::nullopt;
        components[idx++] = v;
    }
    if (idx < 4) return std::nullopt;

    return Theme::ColorToken{components[0], components[1], components[2], components[3]};
}

using ColorMap = std::unordered_map<std::string, Theme::ColorToken>;

// Parses the TOML file and returns a map of fieldName -> ColorToken.
ColorMap ParseThemeToml(const std::string& text)
{
    ColorMap result;
    std::istringstream ss{text};
    std::string line;

    while (std::getline(ss, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        if (trimmed[0] == '[') continue; // section header — ignored

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key   = Trim(trimmed.substr(0, eq));
        std::string value = Trim(trimmed.substr(eq + 1));

        if (auto rgba = ParseRgba(value)) {
            result.emplace(std::move(key), *rgba);
        }
    }
    return result;
}

// Applies colour map overrides to the builder via ColorRole lookup.
void ApplyColorMap(Theme::ThemeBuilder& builder, const ColorMap& map)
{
    auto apply = [&](const char* key, Theme::ColorRole role) {
        if (auto it = map.find(key); it != map.end()) {
            builder.SetColor(role, it->second);
        }
    };

    apply("backgroundPrimary",   Theme::ColorRole::BackgroundPrimary);
    apply("backgroundSecondary", Theme::ColorRole::BackgroundSecondary);
    apply("backgroundTertiary",  Theme::ColorRole::BackgroundTertiary);
    apply("surfaceDefault",      Theme::ColorRole::SurfaceDefault);
    apply("surfaceHover",        Theme::ColorRole::SurfaceHover);
    apply("surfaceActive",       Theme::ColorRole::SurfaceActive);
    apply("borderDefault",       Theme::ColorRole::BorderDefault);
    apply("borderHover",         Theme::ColorRole::BorderHover);
    apply("accentDefault",       Theme::ColorRole::AccentDefault);
    apply("accentHover",         Theme::ColorRole::AccentHover);
    apply("accentActive",        Theme::ColorRole::AccentActive);
    apply("textPrimary",         Theme::ColorRole::TextPrimary);
    apply("textSecondary",       Theme::ColorRole::TextSecondary);
    apply("textDisabled",        Theme::ColorRole::TextDisabled);
    apply("textOnAccent",        Theme::ColorRole::TextOnAccent);
    apply("statusSuccess",       Theme::ColorRole::StatusSuccess);
    apply("statusWarning",       Theme::ColorRole::StatusWarning);
    apply("statusError",         Theme::ColorRole::StatusError);
    apply("statusInfo",          Theme::ColorRole::StatusInfo);
}

} // namespace

// ─── Construction ─────────────────────────────────────────────────────────────

ThemeHotReload::ThemeHotReload() = default;
ThemeHotReload::~ThemeHotReload()
{
    if (_watchHandle != Utility::InvalidWatchHandle) {
        _watcher.Unwatch(_watchHandle);
    }
}

// ─── Configuration ────────────────────────────────────────────────────────────

void ThemeHotReload::SetBaseTheme(const Theme::Theme& base) noexcept
{
    _base = base;
}

void ThemeHotReload::Watch(const Utility::Path& dir)
{
    if (_watchHandle != Utility::InvalidWatchHandle) {
        _watcher.Unwatch(_watchHandle);
        _watchHandle = Utility::InvalidWatchHandle;
    }

    if (auto result = _watcher.Watch(dir)) {
        _watchHandle = *result;
    }
}

// ─── Per-frame ────────────────────────────────────────────────────────────────

void ThemeHotReload::Poll()
{
    _watcher.Poll([this](const Utility::FileEvent& event) {
        if (event.Type != Utility::FileChangeType::Modified &&
            event.Type != Utility::FileChangeType::Created) {
            return;
        }
        const std::string ext = event.ChangedPath.Native().extension().string();
        if (ext != ".toml") return;

        LoadFile(event.ChangedPath);
    });
}

std::optional<Theme::Theme> ThemeHotReload::TakePending() noexcept
{
    return std::exchange(_pending, std::nullopt);
}

// ─── File loading ─────────────────────────────────────────────────────────────

void ThemeHotReload::LoadFile(const Utility::Path& path)
{
    auto result = Utility::File::Read(path);
    if (!result) return;

    const ColorMap map = ParseThemeToml(*result);
    if (map.empty()) return;

    Theme::ThemeBuilder builder{_base};
    ApplyColorMap(builder, map);
    _pending = builder.Build();
}

} // namespace ImFrame::DevTools

#endif // defined(IMF_DEV_TOOLS)
