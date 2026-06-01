/**
 * @file     Timer.hpp
 * @brief    Frame-driven one-shot and repeating callback scheduler
 *
 * `Timer` schedules callbacks driven by explicit `Tick(float dt)` calls — no
 * background threads are involved. The caller controls the time source, which
 * is typically the per-frame delta time from `Application::RunOneFrame()`.
 *
 * Two entry points:
 *  - `After(seconds, callback)` — fires once after the specified delay.
 *  - `Every(interval, callback)` — fires every interval seconds; returns a
 *    `TimerHandle` that cancels the repeating timer when destroyed.
 *
 * New timers scheduled from inside a callback take effect on the next
 * `Tick()` call, not the current one.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.6.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include "ImFrame/Utility/Delegate.hpp"

#include <cstdint>
#include <vector>

namespace ImFrame::Utility {

class Timer;

// ─── TimerHandle ──────────────────────────────────────────────────────────────

/**
 * @class    TimerHandle
 * @brief    RAII handle that cancels a repeating timer when destroyed
 *
 * Returned exclusively by `Timer::Every()`. When the handle goes out of scope
 * the associated repeating timer is automatically cancelled. Move transfers
 * ownership; the moved-from handle becomes a no-op on destruction.
 *
 * @warning  A `TimerHandle` must not outlive its owning `Timer`. Destruction
 *           after the `Timer` is destroyed is undefined behaviour.
 *
 * @since    0.6.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * Timer timer;
 * {
 *     auto handle = timer.Every(1.0f, []() { /* tick *\/ });
 *     timer.Tick(2.5f); // fires twice
 * } // handle out of scope — repeating timer cancelled
 * timer.Tick(1.5f); // no fires
 * @endcode
 */
class [[nodiscard]] TimerHandle {
public:
    /// Constructs an empty (already-cancelled) handle.
    TimerHandle() noexcept = default;

    /**
     * @brief  Destructor — cancels the timer if still active
     */
    ~TimerHandle();

    TimerHandle(const TimerHandle&) = delete;
    TimerHandle& operator=(const TimerHandle&) = delete;

    /**
     * @brief  Move constructor — source becomes empty after the move
     */
    TimerHandle(TimerHandle&& other) noexcept;

    /**
     * @brief  Move-assignment — cancels any existing timer, then takes ownership
     */
    TimerHandle& operator=(TimerHandle&& other) noexcept;

    /**
     * @brief  Cancels the timer immediately, leaving this handle empty
     */
    void Cancel() noexcept;

    /**
     * @brief   Returns true if the timer is still active
     * @return  `true` when the handle references an active timer
     */
    [[nodiscard]] bool IsActive() const noexcept;

private:
    friend class Timer;

    TimerHandle(Timer* timer, std::uint64_t id) noexcept;

    Timer*        _timer{nullptr};
    std::uint64_t _id{0};
};

// ─── Timer ────────────────────────────────────────────────────────────────────

/**
 * @class    Timer
 * @brief    Frame-driven callback scheduler with one-shot and repeating timers
 *
 * Timers are stored in a `std::vector` sorted ascending by next-fire time.
 * `Tick()` is O(k log n) where k is the number of fired timers and n is the
 * total active count.
 *
 * Non-copyable and non-moveable — Timer is intended as a member of
 * `Application` and driven from the main render loop.
 *
 * @since    0.6.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * Timer timer;
 * timer.After(2.0f, []() { std::println("2 s elapsed"); });
 * auto h = timer.Every(0.5f, []() { std::println("0.5 s tick"); });
 *
 * // In the frame loop:
 * timer.Tick(deltaTime);
 * @endcode
 */
class Timer {
public:
    Timer() = default;
    ~Timer() = default;

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    /**
     * @brief    Schedules a one-shot callback after a delay
     *
     * @param[in] seconds   Delay in seconds (must be > 0).
     * @param[in] callback  Callable to invoke once after the delay.
     */
    void After(float seconds, Delegate<void()> callback);

    /**
     * @brief    Schedules a repeating callback at a fixed interval
     *
     * @param[in] interval  Repeat period in seconds (must be > 0).
     * @param[in] callback  Callable to invoke on each interval.
     *
     * @return   `[[nodiscard]]` TimerHandle — store it or the timer is immediately cancelled.
     */
    [[nodiscard]] TimerHandle Every(float interval, Delegate<void()> callback);

    /**
     * @brief    Cancels the timer with the given id
     *
     * Called by `TimerHandle`'s destructor. No-op if the id is not found.
     *
     * @param[in] id  Opaque timer identifier from `Every()`.
     */
    void Cancel(std::uint64_t id) noexcept;

    /**
     * @brief  Cancels all active timers and clears the queue
     */
    void Clear() noexcept;

    /**
     * @brief    Advances time and fires any elapsed timers
     *
     * Fires all timers whose accumulated elapsed time has reached their
     * trigger time. Repeating timers are rescheduled; one-shot timers are
     * removed. New timers scheduled from inside a callback do not fire in
     * this same `Tick()` call.
     *
     * @param[in] dt  Delta time in seconds since last call (must be >= 0).
     */
    void Tick(float dt);

    /**
     * @brief   Returns the number of currently active timers
     * @return  Active timer count (includes both one-shot and repeating)
     */
    [[nodiscard]] std::size_t ActiveCount() const noexcept;

private:
    struct Entry {
        std::uint64_t    id;
        float            triggerAt; ///< Absolute _elapsed when this fires
        float            interval;  ///< 0 = one-shot, > 0 = repeating
        Delegate<void()> callback;
        bool             active{true};
    };

    /// Sorted ascending by triggerAt so Tick() can exit early.
    std::vector<Entry> _entries;
    float              _elapsed{0.0f};
    std::uint64_t      _nextId{1};

    void SortEntries() noexcept;
};

} // namespace ImFrame::Utility
