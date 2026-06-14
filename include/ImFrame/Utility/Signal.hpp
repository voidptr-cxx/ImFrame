/**
 * @file     Signal.hpp
 * @brief    Lightweight observable signal with multi-slot connection support
 *
 * Provides three types:
 *  - `Connection` — RAII handle that disconnects a slot on destruction.
 *  - `Signal<void(Args...)>` — single-threaded multi-slot observable.
 *  - `ThreadSafeSignal<void(Args...)>` — mutex-protected variant for use
 *    when slots may be connected or emitted from different threads.
 *
 * Signal stores its slot list in a shared control block, so Connection handles
 * remain safe (no-op on disconnect) even if the Signal is destroyed first.
 *
 * Emit() copies the slot list before invoking handlers, so connecting or
 * disconnecting during an emission takes effect on the next Emit(), not the
 * current one.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace ImFrame::Utility {

// ─── Connection ───────────────────────────────────────────────────────────────

/**
 * @class    Connection
 * @brief    RAII handle that disconnects a Signal slot when destroyed
 *
 * Returned by `Signal::Connect()` and `ThreadSafeSignal::Connect()`. Must be
 * stored — the `[[nodiscard]]` attribute on Connect() promotes discarding it
 * to a compile warning.
 *
 * Connection is move-only. Moving transfers the disconnect responsibility to
 * the new owner; the moved-from Connection becomes a no-op on destruction.
 *
 * The disconnect operation is always safe, even if the Signal has already been
 * destroyed — the internal closure holds a weak_ptr that is checked first.
 *
 * @since    0.5.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * Signal<void(int)> sig;
 * auto conn = sig.Connect([](int v) { std::println("got {}", v); });
 * sig.Emit(42); // prints "got 42"
 * conn.Disconnect();
 * sig.Emit(1);  // no output
 * @endcode
 */
class [[nodiscard]] Connection {
public:
    /// Constructs an empty (already-disconnected) connection.
    Connection() noexcept = default;

    /**
     * @brief  Destructor — automatically disconnects the slot if still connected
     */
    ~Connection() noexcept;

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    /**
     * @brief  Move constructor — source becomes empty after the move
     */
    Connection(Connection&& other) noexcept;

    /**
     * @brief  Move-assignment — disconnects any existing slot, then takes ownership
     */
    Connection& operator=(Connection&& other) noexcept;

    /**
     * @brief  Disconnects the slot immediately, leaving this connection empty
     */
    void Disconnect() noexcept;

    /**
     * @brief   Returns true if the slot is still connected
     * @return  `true` when this connection holds an active subscription
     */
    [[nodiscard]] bool IsConnected() const noexcept;

private:
    template <typename Sig2> friend class Signal;
    template <typename Sig2> friend class ThreadSafeSignal;

    explicit Connection(std::function<void()> disconnectFn);

    std::function<void()> _disconnectFn;
};

// ─── Signal<void(Args...)> ────────────────────────────────────────────────────

/**
 * @class    Signal
 * @brief    Primary template — undefined; specialise on a void function signature
 *
 * @tparam   Sig  Void function signature, e.g. `void()` or `void(int, float)`
 *
 * @since    0.5.0
 */
template <typename Sig>
class Signal;

/**
 * @class    Signal<void(Args...)>
 * @brief    Lightweight single-threaded observable with RAII slot management
 *
 * Maintains an ordered list of connected handlers. `Connect()` appends a
 * handler and returns a `Connection` RAII handle — the handle must be stored,
 * otherwise the slot is immediately removed. `Emit()` calls all connected
 * handlers in connection order.
 *
 * Signal is not thread-safe. Use `ThreadSafeSignal` when emitting or
 * connecting from multiple threads.
 *
 * Non-copyable and non-moveable: Connection handles hold a weak_ptr to the
 * internal storage; moving Signal would not invalidate those pointers, but
 * we still prevent it for simplicity (consistent with BackgroundWorker/Thread).
 *
 * @tparam   Args  Parameter types of the signal
 *
 * @since    0.5.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * Signal<void(std::string_view)> onRenamed;
 * auto conn = onRenamed.Connect([](std::string_view name) {
 *     std::println("renamed to {}", name);
 * });
 * onRenamed.Emit("main.cpp");
 * @endcode
 */
template <typename... Args>
class Signal<void(Args...)> {
public:
    Signal() : _storage{std::make_shared<Storage>()} {}
    ~Signal() = default;

    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = delete;
    Signal& operator=(Signal&&) = delete;

    /**
     * @brief    Connects a handler and returns an RAII Connection handle
     *
     * The handler is appended to the slot list. The returned Connection must be
     * stored — letting it go out of scope immediately disconnects the handler.
     *
     * @tparam   F       Callable type invocable as `void(Args...)`.
     * @param[in] handler  Callable to connect. Stored by value.
     *
     * @return   `[[nodiscard]]` Connection that disconnects on destruction.
     */
    template <std::invocable<Args...> F>
    [[nodiscard]] Connection Connect(F&& handler) {
        auto id = _storage->nextId++;
        _storage->slots.push_back({id, std::forward<F>(handler)});

        std::weak_ptr<Storage> weak{_storage};
        return Connection{[weak, id]() noexcept {
            if (auto s = weak.lock()) {
                auto& slots = s->slots;
                slots.erase(
                    std::remove_if(slots.begin(), slots.end(),
                        [id](const Slot& slot) { return slot.id == id; }),
                    slots.end());
            }
        }};
    }

    /**
     * @brief    Emits the signal, calling all connected handlers in order
     *
     * The handler list is copied before iteration, so connections made or
     * removed during emission take effect on the next Emit() call.
     *
     * @param[in] args  Arguments forwarded to every connected handler.
     */
    void Emit(Args... args) const {
        auto copy = _storage->slots;
        for (auto& slot : copy) slot.fn(args...);
    }

    /**
     * @brief   Returns the number of currently connected handlers
     * @return  Slot count
     */
    [[nodiscard]] std::size_t ConnectionCount() const noexcept {
        return _storage->slots.size();
    }

    /**
     * @brief  Disconnects all handlers, leaving the signal empty
     */
    void DisconnectAll() noexcept {
        _storage->slots.clear();
    }

private:
    struct Slot {
        std::uint64_t id;
        std::function<void(Args...)> fn;
    };

    struct Storage {
        std::uint64_t nextId{0};
        std::vector<Slot> slots;
    };

    std::shared_ptr<Storage> _storage;
};

// ─── ThreadSafeSignal<void(Args...)> ─────────────────────────────────────────

/**
 * @class    ThreadSafeSignal
 * @brief    Primary template — undefined; specialise on a void function signature
 *
 * @tparam   Sig  Void function signature
 *
 * @since    0.5.0
 */
template <typename Sig>
class ThreadSafeSignal;

/**
 * @class    ThreadSafeSignal<void(Args...)>
 * @brief    Mutex-protected Signal variant safe for concurrent connect and emit
 *
 * Identical API to Signal<void(Args...)> but protects the slot list with a
 * `std::mutex`. Emit() acquires the mutex to copy the handler list, releases
 * it, then invokes handlers without holding the lock — preventing deadlocks
 * when a handler itself calls Connect() or Disconnect().
 *
 * Used by `AnimatedValue<T>` (Phase 12) and `State<T>` (Phase 28) where the
 * signal may be emitted from background threads.
 *
 * @tparam   Args  Parameter types of the signal
 *
 * @since    0.5.0
 */
template <typename... Args>
class ThreadSafeSignal<void(Args...)> {
public:
    ThreadSafeSignal() : _storage{std::make_shared<Storage>()} {}
    ~ThreadSafeSignal() = default;

    ThreadSafeSignal(const ThreadSafeSignal&) = delete;
    ThreadSafeSignal& operator=(const ThreadSafeSignal&) = delete;
    ThreadSafeSignal(ThreadSafeSignal&&) = delete;
    ThreadSafeSignal& operator=(ThreadSafeSignal&&) = delete;

    /**
     * @brief    Connects a handler; thread-safe
     *
     * @tparam   F       Callable type invocable as `void(Args...)`.
     * @param[in] handler  Callable to connect. Stored by value.
     *
     * @return   `[[nodiscard]]` Connection that disconnects on destruction.
     */
    template <std::invocable<Args...> F>
    [[nodiscard]] Connection Connect(F&& handler) {
        std::uint64_t id{};
        {
            std::lock_guard lock{_storage->mutex};
            id = _storage->nextId++;
            _storage->slots.push_back({id, std::forward<F>(handler)});
        }

        std::weak_ptr<Storage> weak{_storage};
        return Connection{[weak, id]() noexcept {
            if (auto s = weak.lock()) {
                std::lock_guard lock{s->mutex};
                auto& slots = s->slots;
                slots.erase(
                    std::remove_if(slots.begin(), slots.end(),
                        [id](const Slot& slot) { return slot.id == id; }),
                    slots.end());
            }
        }};
    }

    /**
     * @brief    Emits the signal; thread-safe
     *
     * Acquires the mutex to copy the handler list, releases it before invoking.
     *
     * @param[in] args  Arguments forwarded to every connected handler.
     */
    void Emit(Args... args) const {
        std::vector<Slot> copy;
        {
            std::lock_guard lock{_storage->mutex};
            copy = _storage->slots;
        }
        for (auto& slot : copy) slot.fn(args...);
    }

    /**
     * @brief   Returns the number of currently connected handlers; thread-safe
     * @return  Slot count at the time of the call
     */
    [[nodiscard]] std::size_t ConnectionCount() const noexcept {
        std::lock_guard lock{_storage->mutex};
        return _storage->slots.size();
    }

    /**
     * @brief  Disconnects all handlers; thread-safe
     */
    void DisconnectAll() noexcept {
        std::lock_guard lock{_storage->mutex};
        _storage->slots.clear();
    }

private:
    struct Slot {
        std::uint64_t id;
        std::function<void(Args...)> fn;
    };

    struct Storage {
        mutable std::mutex mutex;
        std::uint64_t nextId{0};
        std::vector<Slot> slots;
    };

    std::shared_ptr<Storage> _storage;
};

} // namespace ImFrame::Utility
