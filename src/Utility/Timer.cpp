/**
 * @file     Timer.cpp
 * @brief    Timer and TimerHandle implementation
 *
 * @internal
 * Timer::Tick() uses a snapshot-count approach: only the entries present at
 * the start of the tick are considered for firing. New entries appended by
 * callbacks have indices >= snapshot count and are skipped this tick.
 *
 * The entry vector is sorted ascending by triggerAt after every mutation
 * (Tick, After, Every). This allows Tick() to exit early once it reaches an
 * entry whose triggerAt > _elapsed. Inactive entries (one-shots that fired,
 * or cancelled timers) are erased at the end of Tick().
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Utility/Timer.hpp"

#include <algorithm>

namespace ImFrame::Utility {

// ─── TimerHandle ──────────────────────────────────────────────────────────────

TimerHandle::TimerHandle(Timer* timer, std::uint64_t id) noexcept
    : _timer{timer}, _id{id} {}

TimerHandle::~TimerHandle() {
    Cancel();
}

TimerHandle::TimerHandle(TimerHandle&& other) noexcept
    : _timer{other._timer}, _id{other._id} {
    other._timer = nullptr;
    other._id    = 0;
}

TimerHandle& TimerHandle::operator=(TimerHandle&& other) noexcept {
    if (this != &other) {
        Cancel();
        _timer       = other._timer;
        _id          = other._id;
        other._timer = nullptr;
        other._id    = 0;
    }
    return *this;
}

void TimerHandle::Cancel() noexcept {
    if (_timer && _id != 0) {
        _timer->Cancel(_id);
        _timer = nullptr;
        _id    = 0;
    }
}

bool TimerHandle::IsActive() const noexcept {
    return _timer != nullptr && _id != 0;
}

// ─── Timer ────────────────────────────────────────────────────────────────────

void Timer::SortEntries() noexcept {
    std::sort(_entries.begin(), _entries.end(),
              [](const Entry& a, const Entry& b) { return a.triggerAt < b.triggerAt; });
}

void Timer::After(float seconds, Delegate<void()> callback) {
    _entries.push_back({_nextId++, _elapsed + seconds, 0.0f, std::move(callback), true});
    SortEntries();
}

TimerHandle Timer::Every(float interval, Delegate<void()> callback) {
    auto id = _nextId++;
    _entries.push_back({id, _elapsed + interval, interval, std::move(callback), true});
    SortEntries();
    return TimerHandle{this, id};
}

void Timer::Cancel(std::uint64_t id) noexcept {
    for (auto& e : _entries) {
        if (e.id == id) {
            e.active = false;
            return;
        }
    }
}

void Timer::Clear() noexcept {
    _entries.clear();
}

void Timer::Tick(float dt) {
    _elapsed += dt;

    // Only process entries present at the start of this tick. New entries
    // appended from callbacks have index >= snapshotCount and are skipped.
    const auto snapshotCount = static_cast<std::ptrdiff_t>(_entries.size());

    for (std::ptrdiff_t i = 0; i < snapshotCount; ++i) {
        if (!_entries[i].active) continue;
        if (_entries[i].triggerAt > _elapsed) break; // sorted — all remaining > _elapsed

        if (_entries[i].interval <= 0.0f) {
            // One-shot: fire once then deactivate
            auto cb = _entries[i].callback;
            _entries[i].active = false;
            cb();
        } else {
            // Repeating: fire once per elapsed interval (handles large dt gracefully)
            while (_entries[i].active && _entries[i].triggerAt <= _elapsed) {
                auto cb = _entries[i].callback; // re-copy each iteration (cb may modify state)
                _entries[i].triggerAt += _entries[i].interval;
                cb();
            }
        }
    }

    // Remove inactive entries
    _entries.erase(
        std::remove_if(_entries.begin(), _entries.end(),
                       [](const Entry& e) { return !e.active; }),
        _entries.end());

    SortEntries();
}

std::size_t Timer::ActiveCount() const noexcept {
    std::size_t count = 0;
    for (const auto& e : _entries) {
        if (e.active) ++count;
    }
    return count;
}

} // namespace ImFrame::Utility
