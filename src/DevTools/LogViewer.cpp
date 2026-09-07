/**
 * @file     LogViewer.cpp
 * @brief    Log viewer panel — UiSink table with level filtering and text search
 *
 * @internal
 * Level filter checkboxes sit above the table. Text search is a client-side
 * substring match applied each frame to the snapshot returned by UiSink::GetEntries().
 * Auto-scroll is implemented via ImGui::SetScrollHereY(1.0f) after the table loop
 * when the scroll position is already at the bottom or the user has not scrolled up.
 *
 * Level badge colours are drawn as small filled rectangles using ImGui's draw list.
 * The colours are hard-coded to match the Dracula/Nord palette used in Phase 8 themes
 * rather than reading from the active theme to keep the implementation self-contained.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-13
 * @version  1.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#if defined(IMF_DEV_TOOLS)

#include "ImFrame/DevTools/LogViewer.hpp"
#include "ImFrame/App/Window.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>

namespace ImFrame::DevTools {

namespace {

constexpr const char* LEVEL_LABELS[] = { "TRC", "DBG", "INF", "WRN", "ERR", "FTL" };

constexpr ImVec4 LEVEL_COLORS[] = {
    {0.60f, 0.60f, 0.60f, 1.0f}, // Trace  — dim grey
    {0.56f, 0.85f, 0.97f, 1.0f}, // Debug  — cyan
    {0.95f, 0.95f, 0.95f, 1.0f}, // Info   — white
    {0.97f, 0.87f, 0.40f, 1.0f}, // Warn   — yellow
    {0.95f, 0.37f, 0.37f, 1.0f}, // Error  — red
    {1.00f, 0.20f, 0.20f, 1.0f}, // Fatal  — bright red
};

std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp)
{
    const auto tt  = std::chrono::system_clock::to_time_t(tp);
    const auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                         tp.time_since_epoch()).count() % 1000;
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &tt);
#else
    localtime_r(&tt, &local);
#endif
    return std::format("{:02}:{:02}:{:02}.{:03}",
                       local.tm_hour, local.tm_min, local.tm_sec, ms);
}

} // namespace

// ─── Construction ─────────────────────────────────────────────────────────────

LogViewer::LogViewer(std::shared_ptr<Utility::UiSink> sink)
    : _sink(std::move(sink))
{}

// ─── Registration ─────────────────────────────────────────────────────────────

void LogViewer::Register(App::WindowManager& wm)
{
    wm.Register("Log Viewer", &_visible);
}

// ─── Filter helpers ───────────────────────────────────────────────────────────

bool LogViewer::PassesFilter(const Utility::LogEntry& entry) const
{
    if (!_levelFilter[static_cast<int>(entry.level)]) {
        return false;
    }
    if (_searchBuf[0] != '\0') {
        const std::string_view needle{_searchBuf};
        const bool inMsg  = entry.message.find(needle) != std::string::npos;
        const bool inFile = std::string_view{entry.location.file_name()}.find(needle)
                            != std::string_view::npos;
        if (!inMsg && !inFile) {
            return false;
        }
    }
    return true;
}

std::size_t LogViewer::VisibleEntryCount() const
{
    if (!_sink) return 0;
    const auto entries = _sink->GetEntries();
    return static_cast<std::size_t>(
        std::ranges::count_if(entries, [this](const auto& e) { return PassesFilter(e); }));
}

// ─── Render ───────────────────────────────────────────────────────────────────

void LogViewer::Render()
{
    if (!_visible) return;

    ImGui::SetNextWindowSize({700.0f, 400.0f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Log Viewer", &_visible)) {
        ImGui::End();
        return;
    }

    // ── Toolbar ──────────────────────────────────────────────────────────────
    ImGui::Text("Filter:");
    ImGui::SameLine();
    for (int i = 0; i < 6; ++i) {
        ImGui::PushStyleColor(ImGuiCol_Text, LEVEL_COLORS[i]);
        ImGui::Checkbox(LEVEL_LABELS[i], &_levelFilter[i]);
        ImGui::PopStyleColor();
        if (i < 5) ImGui::SameLine();
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("##search", _searchBuf, sizeof(_searchBuf));
    ImGui::SameLine();
    ImGui::Text("Search");

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::Checkbox("Auto-scroll", &_autoScroll);

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Button("Clear") && _sink) {
        _sink->Clear();
    }

    ImGui::Separator();

    // ── Table ────────────────────────────────────────────────────────────────
    constexpr ImGuiTableFlags TABLE_FLAGS =
        ImGuiTableFlags_ScrollY       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_BordersOuter  |
        ImGuiTableFlags_BordersV      |
        ImGuiTableFlags_Resizable     |
        ImGuiTableFlags_SizingStretchProp;

    const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    const ImVec2 tableSize{0.0f, -footerHeight};

    if (!ImGui::BeginTable("##logtable", 4, TABLE_FLAGS, tableSize)) {
        ImGui::End();
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Time",    ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Level",   ImGuiTableColumnFlags_WidthFixed, 44.0f);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Location",ImGuiTableColumnFlags_WidthFixed, 180.0f);
    ImGui::TableHeadersRow();

    if (_sink) {
        const auto entries = _sink->GetEntries();
        for (const auto& entry : entries) {
            if (!PassesFilter(entry)) continue;

            ImGui::TableNextRow();

            // Time
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(FormatTimestamp(entry.timestamp).c_str());

            // Level badge
            ImGui::TableSetColumnIndex(1);
            const int lvlIdx = static_cast<int>(entry.level);
            ImGui::PushStyleColor(ImGuiCol_Text, LEVEL_COLORS[lvlIdx]);
            ImGui::TextUnformatted(LEVEL_LABELS[lvlIdx]);
            ImGui::PopStyleColor();

            // Message
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(entry.message.c_str());

            // Location
            ImGui::TableSetColumnIndex(3);
            const std::string loc = std::format("{}:{}",
                std::filesystem::path(entry.location.file_name()).filename().string(),
                entry.location.line());
            ImGui::TextUnformatted(loc.c_str());
        }

        if (_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }

    ImGui::EndTable();
    ImGui::End();
}

} // namespace ImFrame::DevTools

#endif // defined(IMF_DEV_TOOLS)
