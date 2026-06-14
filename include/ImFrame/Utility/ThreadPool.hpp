/**
 * @file     ThreadPool.hpp
 * @brief    Fixed-size thread pool with a lock-free MPMC task queue
 *
 * Workers are named "<prefix>-0", "<prefix>-1", etc. and use std::jthread
 * throughout. Submit() blocks when the queue is full, providing natural
 * backpressure. The Pimpl pattern keeps the ring-buffer internals out of
 * this public header.
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

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <ranges>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace ImFrame::Utility {

/**
 * @class    ThreadPool
 * @brief    Fixed-size pool of named worker threads sharing a lock-free task queue
 *
 * Workers draw tasks from a cache-line-aligned MPMC ring buffer (Dmitry Vyukov
 * design, capacity 1024). Submit() accepts any callable, deduces its return
 * type, and returns a typed std::future. When the queue is full, Submit()
 * spin-yields and retries until space is available — it never drops tasks.
 *
 * Thread safety: Submit() and SubmitBatch() are safe to call concurrently
 * from multiple threads. WaitAll() and Shutdown() must be called from a
 * single thread and not concurrently with Submit().
 *
 * @note     Uses the Pimpl pattern. The ring-buffer and semaphore internals
 *           live in src/Utility/ThreadPool.cpp and are not visible to consumers.
 *
 * @since    0.4.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * ThreadPool pool{4, "MyPool"};
 * auto f1 = pool.Submit([] { return 42; });
 * auto f2 = pool.Submit([] { return 99; });
 * pool.WaitAll();
 * // f1.get() == 42, f2.get() == 99
 * @endcode
 */
class ThreadPool {
public:
    /**
     * @brief    Constructs a thread pool and starts all worker threads
     *
     * @param[in] workerCount  Number of worker threads.
     *                         Default: std::thread::hardware_concurrency().
     * @param[in] namePrefix   Prefix for worker thread names. Workers are named
     *                         "<prefix>-0", "<prefix>-1", etc. Must not be empty.
     *
     * @throws   std::system_error  If any worker thread cannot be created.
     * @throws   std::bad_alloc     If internal queue or worker storage allocation fails.
     */
    explicit ThreadPool(
        std::size_t workerCount = std::thread::hardware_concurrency(),
        std::string namePrefix  = "ImFrame-Worker");

    /**
     * @brief  Destructor. Calls Shutdown() if not already called
     */
    ~ThreadPool() noexcept;

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    /**
     * @brief    Submits a callable for execution and returns a future for its result
     *
     * Blocks (spin-yields) if the internal queue is full. Never drops a task.
     * The callable is captured by value and executed on an arbitrary worker thread.
     *
     * @tparam   F  Callable type. Must be invocable with no arguments.
     *              Return type is deduced automatically.
     *
     * @param[in] func  The callable to execute. Captured by value.
     *
     * @return   std::future<ReturnType> that becomes ready when func() completes.
     *
     * @throws   std::bad_alloc  If the promise/future pair cannot be allocated.
     */
    template <typename F>
    [[nodiscard]] auto Submit(F&& func) -> std::future<std::invoke_result_t<F>>;

    /**
     * @brief    Submits a range of callables and returns a vector of futures
     *
     * More efficient than individual Submit() calls for bulk work. All callables
     * in the range must share the same return type.
     *
     * @tparam   R  Range type. Value type must be an invocable with a uniform
     *              return type. Must satisfy std::ranges::range.
     *
     * @param[in] range  The range of callables to submit.
     *
     * @return   std::vector of futures, one per callable, in submission order.
     *
     * @throws   std::bad_alloc  If the future vector or any promise cannot be allocated.
     */
    template <std::ranges::range R>
    [[nodiscard]] auto SubmitBatch(R&& range)
        -> std::vector<std::future<std::invoke_result_t<std::ranges::range_value_t<R>>>>;

    /**
     * @brief  Blocks the calling thread until all submitted tasks have completed
     *
     * Submits workerCount sentinel barrier tasks and waits on their futures.
     * Must not be called concurrently with Submit() or from a worker thread.
     */
    void WaitAll();

    /**
     * @brief  Drains the queue, stops accepting new tasks, and joins all workers
     *
     * Called automatically by the destructor. Safe to call explicitly before
     * destruction. Behaviour is undefined if Submit() is called after Shutdown().
     */
    void Shutdown();

    /**
     * @brief  Returns the number of worker threads in this pool
     * @return Worker count as passed to the constructor
     */
    [[nodiscard]] std::size_t WorkerCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;

    /// Pushes a packaged void() task onto the queue, blocking until space is available.
    void PushTask(std::function<void()> task);
};

// ─── Template implementations ─────────────────────────────────────────────────

template <typename F>
auto ThreadPool::Submit(F&& func) -> std::future<std::invoke_result_t<F>> {
    using R = std::invoke_result_t<F>;
    auto promise = std::make_shared<std::promise<R>>();
    auto future  = promise->get_future();

    PushTask([p = std::move(promise), f = std::forward<F>(func)]() mutable {
        if constexpr (std::is_void_v<R>) {
            f();
            p->set_value();
        } else {
            p->set_value(f());
        }
    });

    return future;
}

template <std::ranges::range R>
auto ThreadPool::SubmitBatch(R&& range)
    -> std::vector<std::future<std::invoke_result_t<std::ranges::range_value_t<R>>>> {
    using F      = std::ranges::range_value_t<R>;
    using Result = std::invoke_result_t<F>;

    std::vector<std::future<Result>> futures;
    for (auto&& func : range)
        futures.push_back(Submit(std::forward<decltype(func)>(func)));
    return futures;
}

} // namespace ImFrame::Utility
