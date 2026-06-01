/**
 * @file     ConsoleSink.cpp
 * @brief    ANSI-coloured stderr log sink implementation
 *
 * @internal
 * Color codes are emitted only when stderr is a TTY. On Windows, _isatty() is
 * used; on POSIX, isatty(). ANSI sequences work natively on Windows 10+
 * (Virtual Terminal Processing is enabled by default in recent builds).
 *
 * Format: [HH:MM:SS] LEVEL file:line message
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Utility/Logger.hpp"

#include <cstdio>
#include <ctime>
#include <string_view>

#ifdef _WIN32
  #include <io.h>
  #define IMF_ISATTY(fd) _isatty(fd)
  #define IMF_STDERR_FD  2
#else
  #include <unistd.h>
  #define IMF_ISATTY(fd) isatty(fd)
  #define IMF_STDERR_FD  STDERR_FILENO
#endif

namespace ImFrame::Utility {

namespace {

constexpr std::string_view LevelName(Level l) noexcept {
    switch (l) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "?????";
}

constexpr std::string_view LevelColor(Level l) noexcept {
    switch (l) {
        case Level::Trace: return "\033[2;37m";    // dim white
        case Level::Debug: return "\033[0;36m";    // cyan
        case Level::Info:  return "\033[0;37m";    // white
        case Level::Warn:  return "\033[0;33m";    // yellow
        case Level::Error: return "\033[0;31m";    // red
        case Level::Fatal: return "\033[1;31m";    // bold red
    }
    return "";
}

constexpr std::string_view ANSI_RESET = "\033[0m";

} // namespace

ConsoleSink::ConsoleSink()
    : _isTty{IMF_ISATTY(IMF_STDERR_FD) != 0} {}

void ConsoleSink::Write(const LogEntry& entry) {
    // Format timestamp as HH:MM:SS
    auto tt = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &tt);
#else
    localtime_r(&tt, &tmBuf);
#endif
    char timeBuf[16];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
                  tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);

    if (_isTty) {
        std::fprintf(stderr, "%s[%s] %.*s%s %s:%d %s%s\n",
                     LevelColor(entry.level).data(),
                     timeBuf,
                     static_cast<int>(LevelName(entry.level).size()),
                     LevelName(entry.level).data(),
                     ANSI_RESET.data(),
                     entry.location.file_name(),
                     static_cast<int>(entry.location.line()),
                     entry.message.c_str(),
                     ANSI_RESET.data());
    } else {
        std::fprintf(stderr, "[%s] %.*s %s:%d %s\n",
                     timeBuf,
                     static_cast<int>(LevelName(entry.level).size()),
                     LevelName(entry.level).data(),
                     entry.location.file_name(),
                     static_cast<int>(entry.location.line()),
                     entry.message.c_str());
    }
}

} // namespace ImFrame::Utility
