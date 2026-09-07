/**
 * @file     UiSink.cpp
 * @brief    Fixed-size ring buffer log sink for the in-app viewer (Phase 17)
 *
 * @internal
 * UiSink stores LogEntry values in a std::deque. When the buffer reaches
 * maxEntries, the oldest entry is popped from the front before pushing the
 * new one — O(1) amortised for both operations.
 *
 * All mutating operations lock _mutex. GetEntries() copies under the lock
 * so the caller receives a consistent snapshot. Clear() also locks.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Utility/Logger.hpp"

namespace ImFrame::Utility {

UiSink::UiSink(std::size_t maxEntries)
    : _maxEntries{maxEntries > 0 ? maxEntries : 1} {}

void UiSink::Write(const LogEntry& entry) {
    std::lock_guard lock{_mutex};
    if (_buffer.size() >= _maxEntries) _buffer.pop_front();
    _buffer.push_back(entry);
}

std::vector<LogEntry> UiSink::GetEntries() const {
    std::lock_guard lock{_mutex};
    return {_buffer.begin(), _buffer.end()};
}

void UiSink::Clear() noexcept {
    std::lock_guard lock{_mutex};
    _buffer.clear();
}

} // namespace ImFrame::Utility
