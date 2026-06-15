/**
 * @file     BackgroundWorker.cpp
 * @brief    BackgroundWorker implementation — single FIFO worker with mutex/CV queue
 *
 * @internal
 * The queue is a std::deque<std::function<void()>> protected by a std::mutex.
 * A condition variable (_taskCv) wakes the worker on each Post(). A second
 * condition variable (_drainCv) lets Drain() block until the queue is empty
 * and _inFlight is zero (no task currently executing).
 *
 * The worker thread is owned by a Thread member, which handles OS thread naming
 * automatically from inside the thread function via platform APIs.
 *
 * The FIFO ordering guarantee comes naturally from deque::push_back / pop_front.
 * A lock-free structure would not simplify this because Drain() must coordinate
 * _inFlight atomically with queue emptiness.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-05-31
 * @version  0.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Utility/BackgroundWorker.hpp"

namespace ImFrame::Utility {

// ─── Construction / destruction ───────────────────────────────────────────────

BackgroundWorker::BackgroundWorker(std::string name)
    : _thread(name, [this](std::stop_token st) { WorkerLoop(st); })
// _name is stored in Thread::_name; expose it via Name() by delegating to _thread.
{}

BackgroundWorker::~BackgroundWorker() noexcept {
    // Drain first: ensure every queued task completes before we signal stop.
    Drain();
    _thread.RequestStop();
    _taskCv.notify_all(); // wake the worker so it observes stop_requested()
    // _thread dtor joins automatically (Thread wraps std::jthread).
}

// ─── Accessors ────────────────────────────────────────────────────────────────

const std::string& BackgroundWorker::Name() const noexcept {
    return _thread.Name();
}

// ─── Task submission ──────────────────────────────────────────────────────────

void BackgroundWorker::Enqueue(std::function<void()> task) {
    {
        std::lock_guard lock{_mutex};
        _queue.push_back(std::move(task));
    }
    _taskCv.notify_one();
}

// ─── Drain ────────────────────────────────────────────────────────────────────

void BackgroundWorker::Drain() {
    std::unique_lock lock{_mutex};
    _drainCv.wait(lock, [this] {
        return _queue.empty() && _inFlight == 0;
    });
}

// ─── Worker loop ──────────────────────────────────────────────────────────────

void BackgroundWorker::WorkerLoop(std::stop_token st) {
    while (!st.stop_requested()) {
        std::function<void()> task;

        {
            std::unique_lock lock{_mutex};
            _taskCv.wait(lock, [&] {
                return !_queue.empty() || st.stop_requested();
            });

            if (_queue.empty())
                break; // stop was requested and queue is empty

            task = std::move(_queue.front());
            _queue.pop_front();
            ++_inFlight;
        }

        task();

        {
            std::lock_guard lock{_mutex};
            --_inFlight;
        }
        _drainCv.notify_all();
    }
}

} // namespace ImFrame::Utility
