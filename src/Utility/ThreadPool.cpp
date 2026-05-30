/**
 * @file     ThreadPool.cpp
 * @brief    ThreadPool implementation with MPMC ring-buffer task queue
 *
 * @internal
 * The queue is a cache-line-aligned MPMC ring buffer (Dmitry Vyukov design).
 * Each slot carries a sequence atomic for lock-free coordination. Workers sleep
 * on a std::counting_semaphore and wake one-per-task. This avoids spurious
 * wakeups and minimises mutex contention in the hot dispatch path.
 *
 * Queue capacity is 1024 (power of two). Submit() spin-yields when full,
 * providing natural backpressure without dropping tasks.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-05-31
 * @version  0.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Utility/ThreadPool.hpp"
#include "ImFrame/Utility/Thread.hpp"

#include <algorithm>
#include <cassert>
#include <format>
#include <memory>
#include <new>
#include <semaphore>
#include <vector>

namespace ImFrame::Utility {

// ─── MPMC ring-buffer ─────────────────────────────────────────────────────────

// Suppress MSVC C4324: "structure was padded due to alignment specifier."
// The padding is intentional — it prevents false sharing between head and tail.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

namespace {

static constexpr std::size_t QUEUE_CAPACITY = 1024; // must be a power of two

struct alignas(std::hardware_destructive_interference_size) Slot {
    std::atomic<std::size_t> sequence{0};
    std::function<void()>    task;
};

/// Lock-free MPMC ring buffer (Dmitry Vyukov design).
class MpMcQueue {
public:
    MpMcQueue() {
        for (std::size_t i = 0; i < QUEUE_CAPACITY; ++i)
            _slots[i].sequence.store(i, std::memory_order_relaxed);
    }

    /**
     * Attempts to push a task into the next available slot.
     * Takes task by reference so the caller retains ownership on failure.
     * Returns false if the queue is full (caller should yield and retry).
     */
    bool TryPush(const std::function<void()>& task) {
        std::size_t pos = _tail.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = _slots[pos & (QUEUE_CAPACITY - 1)];
            std::size_t seq = slot.sequence.load(std::memory_order_acquire);
            std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(seq) -
                                  static_cast<std::ptrdiff_t>(pos);
            if (diff == 0) {
                // Slot is ready to accept a new item.
                if (_tail.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed))
                {
                    slot.task = task; // copy into slot; caller retains original on failure
                    slot.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // queue is full
            } else {
                pos = _tail.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * Attempts to pop a task from the next pending slot.
     * Returns false if the queue is empty.
     */
    bool TryPop(std::function<void()>& out) {
        std::size_t pos = _head.load(std::memory_order_relaxed);
        for (;;) {
            Slot& slot = _slots[pos & (QUEUE_CAPACITY - 1)];
            std::size_t seq = slot.sequence.load(std::memory_order_acquire);
            std::ptrdiff_t diff = static_cast<std::ptrdiff_t>(seq) -
                                  static_cast<std::ptrdiff_t>(pos + 1);
            if (diff == 0) {
                // Slot has data ready.
                if (_head.compare_exchange_weak(pos, pos + 1,
                                                std::memory_order_relaxed))
                {
                    out = std::move(slot.task);
                    slot.sequence.store(pos + QUEUE_CAPACITY,
                                        std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false; // queue is empty
            } else {
                pos = _head.load(std::memory_order_relaxed);
            }
        }
    }

private:
    Slot _slots[QUEUE_CAPACITY];
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> _head{0};
    alignas(std::hardware_destructive_interference_size) std::atomic<std::size_t> _tail{0};
};

} // namespace

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// ─── Impl ─────────────────────────────────────────────────────────────────────

struct ThreadPool::Impl {
    MpMcQueue                                    queue;
    std::counting_semaphore<QUEUE_CAPACITY>      semaphore{0};
    std::vector<std::unique_ptr<Thread>>         workers; // unique_ptr because Thread is non-moveable
    std::size_t                                  workerCount{0};
    bool                                         shutdown{false};
};

// ─── Construction / destruction ───────────────────────────────────────────────

ThreadPool::ThreadPool(std::size_t workerCount, std::string namePrefix)
    : _impl(std::make_unique<Impl>()) {
    _impl->workerCount = workerCount;
    _impl->workers.reserve(workerCount);

    for (std::size_t i = 0; i < workerCount; ++i) {
        std::string workerName = std::format("{}-{}", namePrefix, i);
        // Thread is non-moveable, so store via unique_ptr.
        _impl->workers.push_back(
            std::make_unique<Thread>(
                std::move(workerName),
                [this](std::stop_token st) {
                    // Worker loop: acquire the semaphore (one permit per queued task),
                    // then pop and execute. try_acquire_for allows observing stop requests.
                    while (!st.stop_requested()) {
                        if (_impl->semaphore.try_acquire_for(std::chrono::milliseconds{5})) {
                            std::function<void()> task;
                            if (_impl->queue.TryPop(task))
                                task();
                        }
                    }
                }));
    }
}

ThreadPool::~ThreadPool() {
    Shutdown();
}

// ─── Public API ───────────────────────────────────────────────────────────────

std::size_t ThreadPool::WorkerCount() const noexcept {
    return _impl->workerCount;
}

void ThreadPool::PushTask(std::function<void()> task) {
    // Pass task by copy on each retry. std::move would leave task empty after
    // the first failed TryPush, causing an empty std::function to be enqueued
    // on the next successful attempt (which throws on call).
    while (!_impl->queue.TryPush(task))
        std::this_thread::yield();
    _impl->semaphore.release();
}

void ThreadPool::WaitAll() {
    // Submit one barrier task per worker. When all barriers have been popped
    // and their promises fulfilled every previously submitted task is done.
    std::vector<std::future<void>> barriers;
    barriers.reserve(_impl->workerCount);
    for (std::size_t i = 0; i < _impl->workerCount; ++i)
        barriers.push_back(Submit([] {}));
    for (auto& f : barriers)
        f.get();
}

void ThreadPool::Shutdown() {
    if (_impl->shutdown)
        return;
    _impl->shutdown = true;

    // Request all workers to stop and release enough semaphore permits so
    // every worker can unblock from try_acquire_for and see stop_requested().
    for (auto& w : _impl->workers)
        w->RequestStop();
    _impl->semaphore.release(_impl->workerCount);

    // Workers are joined automatically when Thread destructors run.
    _impl->workers.clear();
}

} // namespace ImFrame::Utility
