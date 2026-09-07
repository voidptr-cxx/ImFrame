/**
 * @file     Logger.hpp
 * @brief    Levelled structured logger with async sink dispatch and compile-time filtering
 *
 * Provides:
 *  - `Level` enum — Trace through Fatal
 *  - `LogEntry` — structured log record (level, message, source location, timestamp, thread id)
 *  - `Sink` — pure virtual interface for log destinations
 *  - `ConsoleSink` — ANSI-coloured stderr with TTY detection
 *  - `FileSink` — append-to-file with optional byte-threshold rotation
 *  - `UiSink` — fixed-size ring buffer for the Phase 17 in-app log viewer
 *  - `Logger` — singleton broker; dispatches to sinks via BackgroundWorker
 *  - `IMF_LOG_*` macros — zero-cost compile-time level filtering
 *
 * `Logger::Write()` never blocks the calling thread — all sink I/O runs on the
 * internal `BackgroundWorker`. Call `Logger::Shutdown()` before process exit to
 * drain the worker and flush all sinks.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include "ImFrame/Utility/Path.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

namespace ImFrame::Utility {

// ─── Level ────────────────────────────────────────────────────────────────────

/**
 * @enum     Level
 * @brief    Severity levels for log entries, in ascending order of severity
 * @since    0.6.0
 */
enum class Level : std::uint8_t {
    Trace = 0, ///< Highly verbose diagnostic information
    Debug,     ///< Developer diagnostic information
    Info,      ///< Normal operational messages
    Warn,      ///< Unexpected but recoverable conditions
    Error,     ///< Significant errors that the program can continue from
    Fatal,     ///< Unrecoverable errors
};

// ─── LogEntry ─────────────────────────────────────────────────────────────────

/**
 * @struct   LogEntry
 * @brief    A single structured log record
 * @since    0.6.0
 */
struct LogEntry {
    Level                                  level;     ///< Severity
    std::string                            message;   ///< Formatted message text
    std::source_location                   location;  ///< Call site (file, line, function)
    std::chrono::system_clock::time_point  timestamp; ///< Wall-clock time of the Write() call
    std::thread::id                        threadId;  ///< ID of the thread that called Write()
};

// ─── Sink ─────────────────────────────────────────────────────────────────────

/**
 * @class    Sink
 * @brief    Abstract log destination interface
 *
 * Implement `Write()` to receive log entries. `Flush()` is called by
 * `Logger::Shutdown()` to ensure buffered output is committed.
 *
 * All Sink methods are called on the Logger's BackgroundWorker thread.
 *
 * @since    0.6.0
 */
class Sink {
public:
    virtual ~Sink() = default;

    /**
     * @brief    Receives a log entry and writes it to the destination
     * @param[in] entry  The log record to write.
     */
    virtual void Write(const LogEntry& entry) = 0;

    /**
     * @brief  Flushes any internally buffered output to the destination
     */
    virtual void Flush() {}
};

// ─── ConsoleSink ──────────────────────────────────────────────────────────────

/**
 * @class    ConsoleSink
 * @brief    Writes log entries to stderr with ANSI color codes
 *
 * Color is disabled automatically when stderr is not connected to a TTY
 * (`_isatty()` on Windows, `isatty()` on POSIX).
 *
 * Color scheme:
 *  - Trace: dim white   Debug: cyan
 *  - Info:  white       Warn:  yellow
 *  - Error: red         Fatal: bold red
 *
 * @since    0.6.0
 *
 * @example
 * @code
 * auto& log = ImFrame::Utility::Logger::Instance();
 * log.AddSink(std::make_shared<ImFrame::Utility::ConsoleSink>());
 * @endcode
 */
class ConsoleSink : public Sink {
public:
    ConsoleSink();
    void Write(const LogEntry& entry) override;

private:
    bool _isTty;
};

// ─── FileSink ─────────────────────────────────────────────────────────────────

/**
 * @class    FileSink
 * @brief    Appends log entries to a file with optional size-based rotation
 *
 * When `rotateAtBytes` is non-zero and the current log file reaches that
 * threshold, the file is renamed to `<stem>.1.log` and a new file is opened.
 * Only one rotated file is kept (previous `.1.log` is overwritten).
 *
 * @since    0.6.0
 *
 * @example
 * @code
 * auto sink = std::make_shared<ImFrame::Utility::FileSink>(
 *     Path{"logs/app.log"}, 10 * 1024 * 1024 /* 10 MiB *\/);
 * ImFrame::Utility::Logger::Instance().AddSink(std::move(sink));
 * @endcode
 */
class FileSink : public Sink {
public:
    /**
     * @brief    Constructs a FileSink that writes to `path`
     *
     * Opens the file for appending immediately. Parent directories must exist.
     *
     * @param[in] path           Destination file path.
     * @param[in] rotateAtBytes  Byte threshold for rotation. 0 = no rotation.
     *
     * @throws   std::runtime_error  If the file cannot be opened.
     */
    explicit FileSink(const Path& path, std::size_t rotateAtBytes = 0);

    /**
     * @brief  Destructor — flushes and closes the file
     */
    ~FileSink() noexcept;

    void Write(const LogEntry& entry) override;
    void Flush() override;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

// ─── UiSink ───────────────────────────────────────────────────────────────────

/**
 * @class    UiSink
 * @brief    Fixed-size ring buffer sink for the in-app log viewer (Phase 17)
 *
 * Stores log entries in a `std::deque` capped at `maxEntries`. When the buffer
 * is full, the oldest entry is discarded. No I/O is performed.
 *
 * `GetEntries()` returns a snapshot copy; access is protected by a mutex so it
 * is safe to call from the render thread while the Logger's BackgroundWorker
 * writes entries concurrently.
 *
 * @since    0.6.0
 *
 * @example
 * @code
 * auto uiSink = std::make_shared<ImFrame::Utility::UiSink>(500);
 * ImFrame::Utility::Logger::Instance().AddSink(uiSink);
 * // Later, on the render thread:
 * for (const auto& entry : uiSink->GetEntries()) { renderLogLine(entry); }
 * @endcode
 */
class UiSink : public Sink {
public:
    /**
     * @brief    Constructs a UiSink with a ring buffer of at most `maxEntries`
     * @param[in] maxEntries  Maximum number of log entries to retain. Default: 1000.
     */
    explicit UiSink(std::size_t maxEntries = 1000);

    void Write(const LogEntry& entry) override;

    /**
     * @brief   Returns a snapshot of the currently buffered entries
     *
     * Thread-safe — safe to call from the render thread concurrently with Write().
     *
     * @return  Copy of the ring buffer contents, oldest-first.
     */
    [[nodiscard]] std::vector<LogEntry> GetEntries() const;

    /**
     * @brief  Clears all buffered entries; thread-safe
     */
    void Clear() noexcept;

private:
    mutable std::mutex    _mutex;
    std::deque<LogEntry>  _buffer;
    std::size_t           _maxEntries;
};

// ─── Logger ───────────────────────────────────────────────────────────────────

/**
 * @class    Logger
 * @brief    Singleton levelled logger with BackgroundWorker-backed async dispatch
 *
 * Use `Logger::Instance()` to obtain the process-wide singleton. Add sinks
 * before logging. The calling thread never blocks on I/O — all sink writes
 * execute on the internal BackgroundWorker thread.
 *
 * Use the `IMF_LOG_*` macros (e.g. `IMF_INFO(...)`) rather than calling
 * `Logger::Write()` directly — they capture `std::source_location` and apply
 * compile-time level filtering.
 *
 * Call `Shutdown()` before process exit to ensure all queued entries reach
 * their sinks.
 *
 * @since    0.6.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * auto& log = Logger::Instance();
 * log.AddSink(std::make_shared<ConsoleSink>());
 * log.AddSink(std::make_shared<FileSink>(Path{"app.log"}));
 *
 * IMF_INFO("Application started v{}.{}", major, minor);
 * IMF_WARN("Config file not found, using defaults");
 *
 * log.Shutdown(); // before exit
 * @endcode
 */
class Logger {
public:
    /**
     * @brief  Constructs a Logger with no sinks and default minimum level
     *
     * @throws std::system_error  If the BackgroundWorker thread cannot start.
     */
    Logger();

    /**
     * @brief  Destructor — does NOT drain the worker; call Shutdown() first
     */
    ~Logger() noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    /**
     * @brief   Returns the process-wide singleton Logger
     * @return  Reference to the singleton (created on first call — Meyer's singleton)
     */
    [[nodiscard]] static Logger& Instance();

    /**
     * @brief    Registers a sink to receive log entries
     *
     * Sinks are invoked in registration order on the worker thread.
     *
     * @param[in] sink  Shared-ownership sink. Must not be null.
     */
    void AddSink(std::shared_ptr<Sink> sink);

    /**
     * @brief    Removes a previously registered sink
     *
     * No-op if the sink is not registered.
     *
     * @param[in] sink  The sink to remove (matched by pointer equality).
     */
    void RemoveSink(const std::shared_ptr<Sink>& sink);

    /**
     * @brief    Sets the runtime minimum log level
     *
     * Entries below this level are discarded before being posted to the worker.
     * The compile-time `IMF_LOG_LEVEL_MIN` filter is applied earlier and is
     * independent of this runtime filter.
     *
     * @param[in] minLevel  Entries with level < minLevel are discarded.
     */
    void SetMinLevel(Level minLevel) noexcept;

    /**
     * @brief    Writes a log entry, dispatching to sinks on the worker thread
     *
     * Prefer the `IMF_LOG_*` macros — they capture source location automatically.
     * This method returns immediately without waiting for sink I/O.
     *
     * @param[in] level    Severity of the entry.
     * @param[in] message  Pre-formatted message string.
     * @param[in] loc      Call-site location (default: caller's source location).
     */
    void Write(Level level, std::string message,
               std::source_location loc = std::source_location::current());

    /**
     * @brief  Drains the worker and flushes all sinks
     *
     * Blocks until all pending entries have been written and flushed.
     * Call before process exit or before removing sinks.
     */
    void Shutdown();

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;
};

} // namespace ImFrame::Utility

// ─── IMF_LOG_* macros ─────────────────────────────────────────────────────────

/**
 * @def      IMF_LOG_LEVEL_MIN
 * @brief    Compile-time minimum log level (integer). Entries below this level
 *           compile to complete no-ops with zero runtime cost.
 *
 * Override by defining `IMF_LOG_LEVEL_MIN` before including this header or via
 * a compiler flag. Values: 0=Trace 1=Debug 2=Info 3=Warn 4=Error 5=Fatal.
 * Default: 1 (Debug) in debug builds, 2 (Info) in release.
 */
#ifndef IMF_LOG_LEVEL_MIN
  #ifdef NDEBUG
    #define IMF_LOG_LEVEL_MIN 2
  #else
    #define IMF_LOG_LEVEL_MIN 1
  #endif
#endif

/// @cond INTERNAL
#define IMF_LOG_IMPL(lvl, ...) \
    do { \
        if constexpr (static_cast<int>(lvl) >= IMF_LOG_LEVEL_MIN) { \
            ::ImFrame::Utility::Logger::Instance().Write(lvl, std::format(__VA_ARGS__)); \
        } \
    } while (false)
/// @endcond

/** @brief  Logs at Trace level. Compile-time no-op if below IMF_LOG_LEVEL_MIN. */
#define IMF_TRACE(...) IMF_LOG_IMPL(::ImFrame::Utility::Level::Trace, __VA_ARGS__)

/** @brief  Logs at Debug level. Compile-time no-op if below IMF_LOG_LEVEL_MIN. */
#define IMF_DEBUG(...) IMF_LOG_IMPL(::ImFrame::Utility::Level::Debug, __VA_ARGS__)

/** @brief  Logs at Info level. Compile-time no-op if below IMF_LOG_LEVEL_MIN. */
#define IMF_INFO(...)  IMF_LOG_IMPL(::ImFrame::Utility::Level::Info,  __VA_ARGS__)

/** @brief  Logs at Warn level. Compile-time no-op if below IMF_LOG_LEVEL_MIN. */
#define IMF_WARN(...)  IMF_LOG_IMPL(::ImFrame::Utility::Level::Warn,  __VA_ARGS__)

/** @brief  Logs at Error level. Compile-time no-op if below IMF_LOG_LEVEL_MIN. */
#define IMF_ERROR(...) IMF_LOG_IMPL(::ImFrame::Utility::Level::Error, __VA_ARGS__)

/** @brief  Logs at Fatal level. Compile-time no-op if below IMF_LOG_LEVEL_MIN. */
#define IMF_FATAL(...) IMF_LOG_IMPL(::ImFrame::Utility::Level::Fatal, __VA_ARGS__)
