/**
 * @file     BackgroundWorker.hpp
 * @brief    Single dedicated worker thread with a FIFO task queue
 *
 * Unlike ThreadPool, BackgroundWorker guarantees task execution in submission
 * order. Intended for use by Logger and Config (Phase 6) for serialised async
 * I/O. Tasks are dispatched via Post() (fire-and-forget) or PostAwaitable()
 * (returns a std::future). Non-copyable and non-moveable.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-05-31
 * @version  0.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Utility/Thread.hpp"

#include <concepts>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <type_traits>

namespace ImFrame::Utility {

/**
 * @class    BackgroundWorker
 * @brief    Single worker thread with a FIFO mutex-protected task queue
 *
 * Tasks submitted via Post() or PostAwaitable() are guaranteed to execute in
 * submission order. The worker thread sleeps on a condition variable when the
 * queue is empty and wakes on each Post(). Drain() blocks the caller until the
 * queue is empty and the in-flight task (if any) has completed.
 *
 * Internally wraps a Thread so the OS thread name is set automatically
 * (visible in debuggers and profilers) without duplicating platform logic.
 *
 * Thread safety: Post() and PostAwaitable() are safe to call concurrently from
 * multiple threads. Drain() must be called from a single external thread and
 * must not be called from within a submitted task.
 *
 * @note     The destructor calls Drain() before requesting the worker to stop,
 *           ensuring all queued tasks complete before the object is destroyed.
 *           New tasks must not be posted after the destructor begins.
 *
 * @since    0.4.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * BackgroundWorker worker{"LogWriter"};
 * worker.Post([] { // write a log entry });
 * auto checksum = worker.PostAwaitable([] { return computeChecksum(); });
 * worker.Drain();
 * // checksum.get() is now safe to call
 * @endcode
 */
class BackgroundWorker {
public:
    /**
     * @brief    Constructs and starts a named background worker thread
     *
     * @param[in] name  Human-readable name for the worker thread, applied via
     *                  platform thread-naming APIs (visible in debuggers).
     *
     * @throws   std::system_error  If the worker thread cannot be created.
     */
    explicit BackgroundWorker(std::string name);

    /**
     * @brief  Destructor. Drains the queue then joins the worker thread
     *
     * @note   Blocks until all previously queued tasks have completed.
     *         New tasks must not be posted after the destructor begins.
     */
    ~BackgroundWorker() noexcept;

    BackgroundWorker(const BackgroundWorker&)            = delete;
    BackgroundWorker& operator=(const BackgroundWorker&) = delete;
    BackgroundWorker(BackgroundWorker&&)                 = delete;
    BackgroundWorker& operator=(BackgroundWorker&&)      = delete;

    /**
     * @brief  Returns the name passed at construction
     * @return The worker thread's human-readable name
     */
    [[nodiscard]] const std::string& Name() const noexcept;

    /**
     * @brief    Posts a fire-and-forget task to the worker queue
     *
     * Returns immediately. The task executes asynchronously in submission order.
     *
     * @tparam   F       Callable type. Must be invocable with no arguments.
     * @param[in] func   The callable to queue. Captured by value.
     */
    template <std::invocable F>
    void Post(F&& func);

    /**
     * @brief    Posts a task and returns a future for its result
     *
     * Returns immediately. The future becomes ready when the task completes.
     *
     * @tparam   F  Callable type. Must be invocable with no arguments.
     *
     * @param[in] func  The callable to queue. Captured by value.
     *
     * @return   std::future<ReturnType> that becomes ready on task completion.
     *
     * @throws   std::bad_alloc  If the promise/future pair cannot be allocated.
     */
    template <std::invocable F>
    [[nodiscard]] auto PostAwaitable(F&& func) -> std::future<std::invoke_result_t<F>>;

    /**
     * @brief  Blocks until the task queue is empty and no task is in-flight
     *
     * Safe to call from any thread. Returns immediately if the queue is already
     * empty and the worker is idle. Must not be called from within a posted task.
     */
    void Drain();

private:
    void WorkerLoop(std::stop_token st);

    /// Appends a pre-packaged void() task to the queue and notifies the worker.
    void Enqueue(std::function<void()> task);

    // Members captured by the worker lambda must be declared before _thread.
    std::mutex                        _mutex;
    std::condition_variable           _taskCv;  ///< Worker wakes when a task is posted
    std::condition_variable           _drainCv; ///< Drain() waits until the worker is idle
    std::deque<std::function<void()>> _queue;
    std::size_t                       _inFlight{0};
    Thread                            _thread;  ///< Declared last — owns the OS thread
};

// ─── Template implementations ─────────────────────────────────────────────────

template <std::invocable F>
void BackgroundWorker::Post(F&& func) {
    Enqueue([f = std::forward<F>(func)]() mutable { f(); });
}

template <std::invocable F>
auto BackgroundWorker::PostAwaitable(F&& func) -> std::future<std::invoke_result_t<F>> {
    using R      = std::invoke_result_t<F>;
    auto promise = std::make_shared<std::promise<R>>();
    auto future  = promise->get_future();

    Enqueue([p = std::move(promise), f = std::forward<F>(func)]() mutable {
        if constexpr (std::is_void_v<R>) {
            f();
            p->set_value();
        } else {
            p->set_value(f());
        }
    });

    return future;
}

} // namespace ImFrame::Utility
