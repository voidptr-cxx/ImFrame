/**
 * @file     Logger.cpp
 * @brief    Logger singleton implementation with BackgroundWorker async dispatch
 *
 * @internal
 * Logger::Impl holds the BackgroundWorker, sink list, and runtime min-level.
 * Write() constructs a LogEntry on the calling thread (capturing the source
 * location and timestamp at the call site), then posts it to the worker.
 * The worker calls DispatchToSinks() which acquires a shared lock on the
 * sinks list, copies it, releases the lock, then calls each Sink::Write()
 * without holding the lock — preventing deadlocks from sink callbacks.
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
#include "ImFrame/Utility/BackgroundWorker.hpp"

#include <algorithm>
#include <atomic>
#include <shared_mutex>

namespace ImFrame::Utility {

// ─── Logger::Impl ─────────────────────────────────────────────────────────────

struct Logger::Impl {
    mutable std::shared_mutex           sinksMutex;
    std::vector<std::shared_ptr<Sink>>  sinks;
    std::atomic<Level>                  minLevel{Level::Debug};
    BackgroundWorker                    worker{"Logger"};

    void DispatchToSinks(const LogEntry& entry) {
        std::vector<std::shared_ptr<Sink>> copy;
        {
            std::shared_lock lock{sinksMutex};
            copy = sinks;
        }
        for (auto& sink : copy) sink->Write(entry);
    }
};

// ─── Logger ───────────────────────────────────────────────────────────────────

Logger::Logger()
    : _impl{std::make_shared<Impl>()} {}

Logger::~Logger() = default;

Logger& Logger::Instance() {
    static Logger singleton;
    return singleton;
}

void Logger::AddSink(std::shared_ptr<Sink> sink) {
    std::unique_lock lock{_impl->sinksMutex};
    _impl->sinks.push_back(std::move(sink));
}

void Logger::RemoveSink(const std::shared_ptr<Sink>& sink) {
    std::unique_lock lock{_impl->sinksMutex};
    auto& s = _impl->sinks;
    s.erase(std::remove(s.begin(), s.end(), sink), s.end());
}

void Logger::SetMinLevel(Level minLevel) noexcept {
    _impl->minLevel.store(minLevel, std::memory_order_relaxed);
}

void Logger::Write(Level level, std::string message, std::source_location loc) {
    if (level < _impl->minLevel.load(std::memory_order_relaxed)) return;

    LogEntry entry{
        level,
        std::move(message),
        loc,
        std::chrono::system_clock::now(),
        std::this_thread::get_id(),
    };

    _impl->worker.Post([impl = _impl, e = std::move(entry)]() {
        impl->DispatchToSinks(e);
    });
}

void Logger::Shutdown() {
    _impl->worker.Drain();
    std::vector<std::shared_ptr<Sink>> copy;
    {
        std::shared_lock lock{_impl->sinksMutex};
        copy = _impl->sinks;
    }
    for (auto& sink : copy) sink->Flush();
}

} // namespace ImFrame::Utility
