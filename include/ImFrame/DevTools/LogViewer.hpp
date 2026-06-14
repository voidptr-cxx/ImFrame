/**
 * @file     LogViewer.hpp
 * @brief    In-app log viewer panel backed by UiSink's ring buffer
 *
 * Renders a docked developer panel containing a table of log entries drawn from
 * a `UiSink` ring buffer. Level filter checkboxes and a text search field are
 * applied client-side each frame. Auto-scroll keeps the newest entry visible.
 *
 * The entire class is conditionally compiled: it only exists when `IMF_DEV_TOOLS`
 * is defined. In release builds the header compiles to nothing.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-13
 * @version  1.7.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#if defined(IMF_DEV_TOOLS)

#include "ImFrame/Utility/Logger.hpp"

#include <array>
#include <memory>

namespace ImFrame::App { class WindowManager; }

namespace ImFrame::DevTools {

/**
 * @class    LogViewer
 * @brief    Developer panel displaying the UiSink log buffer with level filtering and text search
 *
 * Register once via `Register()` so the panel appears in the application's View
 * menu. Call `Render()` every frame from an `OnUi` callback; the panel is only
 * drawn when its visibility flag is `true`.
 *
 * Columns rendered: Timestamp · Level badge · Message · File:Line.
 *
 * @since    1.7.0
 *
 * @example
 * @code
 * auto sink = std::make_shared<ImFrame::Utility::UiSink>(1000);
 * ImFrame::Utility::Logger::Instance().AddSink(sink);
 *
 * ImFrame::DevTools::LogViewer viewer{sink};
 * viewer.Register(app.GetWindowManager());
 * app.OnUi([&] { viewer.Render(); });
 * @endcode
 */
class LogViewer {
public:
    /**
     * @brief    Constructs a LogViewer bound to the given UiSink
     * @param[in] sink  Ring-buffer sink populated by the Logger. Must not be null.
     */
    explicit LogViewer(std::shared_ptr<Utility::UiSink> sink);

    /**
     * @brief    Registers this viewer with a WindowManager for View-menu visibility control
     *
     * The "Log Viewer" item appears in the application's View menu after this
     * call. The visibility flag is owned by LogViewer.
     *
     * @param[in,out] wm  The application's WindowManager.
     */
    void Register(App::WindowManager& wm);

    /**
     * @brief  Renders the log viewer panel; call once per frame from an OnUi callback
     *
     * No-op when the panel is not visible.
     */
    void Render();

    /**
     * @brief  Returns the number of entries currently matching the active filters
     *
     * Useful in tests to verify that filtering reduces the visible row count.
     *
     * @return  Count of entries that pass both level filter and text search.
     */
    [[nodiscard]] std::size_t VisibleEntryCount() const;

private:
    std::shared_ptr<Utility::UiSink> _sink;
    bool                             _visible    = true;
    std::array<bool, 6>              _levelFilter{true, true, true, true, true, true};
    char                             _searchBuf[256]{};
    bool                             _autoScroll = true;

    [[nodiscard]] bool PassesFilter(const Utility::LogEntry& entry) const;
};

} // namespace ImFrame::DevTools

#endif // defined(IMF_DEV_TOOLS)
