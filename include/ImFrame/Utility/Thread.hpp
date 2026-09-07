/**
 * @file     Thread.hpp
 * @brief    RAII thread wrapper with platform naming, priority, and CPU affinity
 *
 * Wraps std::jthread with OS-level thread naming (visible in debuggers and
 * profilers), thread priority control, and CPU affinity pinning. The wrapped
 * thread joins automatically on destruction. Non-copyable and non-moveable.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-05-31
 * @version  0.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <string>
#include <thread>

namespace ImFrame::Utility {

/**
 * @enum     Priority
 * @brief    Scheduling priority hint applied to a Thread
 *
 * @since    0.4.0
 */
enum class Priority {
    Low,      ///< Below-normal priority. Suitable for background housekeeping.
    Normal,   ///< Default OS scheduling priority.
    High,     ///< Above-normal priority. Use with care — may starve Normal threads.
    Realtime  ///< Real-time scheduling (SCHED_FIFO / THREAD_PRIORITY_TIME_CRITICAL).
              ///< Requires elevated privileges on Linux (CAP_SYS_NICE).
};

/**
 * @class    Thread
 * @brief    RAII wrapper around std::jthread with OS-level naming and priority control
 *
 * Thread names are applied via platform APIs immediately after the thread
 * starts executing (pthread_setname_np on POSIX, SetThreadDescription on
 * Windows) and appear in debuggers, profilers, and the OS task manager.
 * The destructor requests a stop and joins automatically via std::jthread —
 * no explicit join is needed.
 *
 * Thread is non-copyable and non-moveable. The constructor lambda captures
 * internal state by address; moving the object would invalidate those captures.
 * Member declaration order — _name, _running, _thread — ensures that _name
 * and _running are valid before _thread's constructor starts the OS thread.
 *
 * @note     On Linux the kernel truncates thread names to 15 characters.
 *           Names longer than 15 characters are silently truncated.
 *
 * @warning  Do not use Priority::Realtime unless you understand the risk of
 *           starving all other threads on the same core.
 *
 * @since    0.4.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * Thread worker{"MyWorker", [](std::stop_token st) {
 *     while (!st.stop_requested()) {
 *         // do work…
 *     }
 * }};
 * worker.SetPriority(Priority::High);
 * // worker joins automatically when it goes out of scope
 * @endcode
 */
class Thread {
public:
    /**
     * @brief    Constructs and immediately starts a named thread
     *
     * The callable is wrapped to set the OS thread name and track the running
     * state. If F is invocable with std::stop_token the token is forwarded;
     * otherwise the callable is invoked with no arguments.
     *
     * @tparam   F       Callable type. Must be invocable with () or (std::stop_token).
     * @param[in] name   Human-readable thread name, visible in debuggers and profilers.
     *                   On Linux, truncated to 15 characters by the kernel.
     * @param[in] func   The callable to run on the new thread. Moved into the thread.
     *
     * @throws   std::system_error  If the underlying std::jthread cannot be created.
     */
    template <typename F>
    explicit Thread(std::string name, F&& func);

    Thread(const Thread&)            = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&&)                 = delete;
    Thread& operator=(Thread&&)      = delete;

    /**
     * @brief  Returns the name passed at construction
     * @return The thread's human-readable name
     */
    [[nodiscard]] const std::string& Name() const noexcept;

    /**
     * @brief  Returns the OS thread identifier
     * @return std::thread::id of the underlying jthread
     */
    [[nodiscard]] std::thread::id Id() const noexcept;

    /**
     * @brief  Returns true while the thread callable is executing
     * @return true if the callable has started and has not yet returned; false otherwise
     */
    [[nodiscard]] bool IsRunning() const noexcept;

    /**
     * @brief  Delivers a cooperative stop request via std::stop_token
     *
     * The running callable must observe std::stop_token::stop_requested() and
     * exit voluntarily. Returns immediately without waiting for the thread to finish.
     */
    void RequestStop() noexcept;

    /**
     * @brief    Applies an OS scheduling priority to this thread
     *
     * May silently fail on platforms where the requested priority requires
     * privileges the process does not hold. Priority::Realtime logs a warning
     * once Logger is available (Phase 6).
     *
     * @param[in] priority  The desired scheduling priority.
     */
    void SetPriority(Priority priority);

    /**
     * @brief    Pins the thread to specific CPU cores
     *
     * @param[in] coreMask  Bitmask of cores to allow. Bit 0 = core 0, bit 1 = core 1.
     *                      On macOS this is best-effort via THREAD_AFFINITY_POLICY.
     *                      Silently ignored on platforms that do not support affinity.
     */
    void SetAffinity(uint32_t coreMask);

private:
    /// Sets the OS-level name of the calling thread. Platform implementation in Thread.cpp.
    static void ApplyCurrentThreadName(const std::string& name);

    std::string       _name;
    std::atomic<bool> _running{false};
    std::jthread      _thread; ///< Declared last — constructor captures &_name and &_running
};

// ─── Template implementation ──────────────────────────────────────────────────

template <typename F>
Thread::Thread(std::string name, F&& func)
    : _name(std::move(name))
    , _running(false)
    , _thread([this, f = std::forward<F>(func)](std::stop_token st) mutable {
        _running.store(true, std::memory_order_release);
        ApplyCurrentThreadName(_name);
        if constexpr (std::invocable<F, std::stop_token>)
            f(st);
        else
            f();
        _running.store(false, std::memory_order_release);
    })
{}

} // namespace ImFrame::Utility
