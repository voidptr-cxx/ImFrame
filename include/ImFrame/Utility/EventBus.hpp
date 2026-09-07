/**
 * @file     EventBus.hpp
 * @brief    Type-safe publish-subscribe event bus for decoupled component communication
 *
 * EventBus is a thread-safe typed pub/sub broker. Events are plain structs —
 * any aggregate type is a valid event; no base class or registration macro is
 * required. Events are identified internally by `std::type_index`, giving O(1)
 * dispatch lookup.
 *
 * Usage:
 *  - `Subscribe<E>(handler)` — register a handler; returns a `SubscriptionToken`
 *    that must be stored. Destruction of the token automatically unsubscribes.
 *  - `Dispatch<E>(event)` — invoke all handlers synchronously on the calling thread.
 *  - `DispatchAsync<E>(event)` — post the dispatch to a BackgroundWorker; handlers
 *    run on the worker thread and must be thread-safe.
 *
 * Dispatch() is re-entrant: the handler list is copied before iteration under
 * a shared lock, then handlers are called without holding any lock.
 * Subscribe()/Unsubscribe() within a handler are safe.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>

namespace ImFrame::Utility {

// ─── Forward declarations ─────────────────────────────────────────────────────

class EventBus;

// ─── SubscriptionToken ────────────────────────────────────────────────────────

/**
 * @class    SubscriptionToken
 * @brief    RAII handle that unsubscribes a handler when destroyed
 *
 * Returned exclusively by `EventBus::Subscribe()`. Must be stored — the
 * `[[nodiscard]]` attribute on Subscribe() makes discarding it a compile
 * warning (promoted to error in CI via `-Werror`).
 *
 * SubscriptionToken is move-only. Moving transfers ownership; the moved-from
 * token becomes a no-op on destruction. Token destruction after the owning
 * EventBus has been destroyed is always safe — the internal reference to the
 * bus state is a weak reference that is checked before any call.
 *
 * @since    0.5.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 * struct Renamed { std::string newName; };
 *
 * EventBus bus;
 * auto token = bus.Subscribe<Renamed>([](const Renamed& e) {
 *     std::println("renamed to {}", e.newName);
 * });
 * bus.Dispatch(Renamed{"main.cpp"}); // prints "renamed to main.cpp"
 * // token goes out of scope → handler is unsubscribed automatically
 * @endcode
 */
class [[nodiscard]] SubscriptionToken {
public:
    /// Constructs an empty (already-unsubscribed) token.
    SubscriptionToken() noexcept = default;

    /**
     * @brief  Destructor — unsubscribes the handler if still active
     */
    ~SubscriptionToken() noexcept;

    SubscriptionToken(const SubscriptionToken&) = delete;
    SubscriptionToken& operator=(const SubscriptionToken&) = delete;

    /**
     * @brief  Move constructor — source becomes empty after the move
     */
    SubscriptionToken(SubscriptionToken&& other) noexcept;

    /**
     * @brief  Move-assignment — unsubscribes any existing handler, then takes ownership
     */
    SubscriptionToken& operator=(SubscriptionToken&& other) noexcept;

    /**
     * @brief  Unsubscribes the handler immediately, leaving this token empty
     */
    void Unsubscribe() noexcept;

    /**
     * @brief   Returns true if the handler is still subscribed
     * @return  `true` when this token holds an active subscription
     */
    [[nodiscard]] bool IsSubscribed() const noexcept;

private:
    friend class EventBus;

    explicit SubscriptionToken(std::function<void()> unsubscribeFn);

    std::function<void()> _unsubscribeFn;
};

// ─── EventBus ─────────────────────────────────────────────────────────────────

/**
 * @class    EventBus
 * @brief    Thread-safe typed publish-subscribe broker
 *
 * Events are identified by `std::type_index` — any plain struct is a valid
 * event type. Handlers are `void(const EventType&)` callables.
 *
 * Thread safety:
 *  - Subscribe() and Unsubscribe() (via token destruction) are safe from any
 *    thread; they acquire an exclusive lock on the per-type handler list.
 *  - Dispatch() is safe from multiple threads simultaneously; it acquires a
 *    shared lock, copies the handler list, releases the lock, then invokes.
 *  - DispatchAsync() is always safe; it posts a copy of the event to an
 *    internal BackgroundWorker.
 *
 * EventBus can be used as a non-singleton instance (e.g. per subsystem) or
 * accessed globally via `EventBus::Instance()` (Meyer's singleton).
 *
 * @since    0.5.0
 *
 * @example
 * @code
 * using namespace ImFrame::Utility;
 *
 * struct WindowResized { int width, height; };
 *
 * // Global singleton
 * auto& bus = EventBus::Instance();
 * auto token = bus.Subscribe<WindowResized>([](const WindowResized& e) {
 *     std::println("window {}x{}", e.width, e.height);
 * });
 * bus.Dispatch(WindowResized{1920, 1080});
 *
 * // Async — handler runs on the internal BackgroundWorker thread
 * bus.DispatchAsync(WindowResized{800, 600});
 * @endcode
 */
class EventBus {
public:
    /**
     * @brief  Constructs a standalone EventBus instance
     *
     * @throws std::system_error  If the internal BackgroundWorker thread cannot start.
     */
    EventBus();

    /**
     * @brief  Destructor — waits for any in-flight async dispatches to complete
     */
    ~EventBus() noexcept;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    /**
     * @brief   Returns the process-wide singleton EventBus
     * @return  Reference to the singleton, created on first call (Meyer's singleton)
     */
    [[nodiscard]] static EventBus& Instance();

    // ── Subscribe ─────────────────────────────────────────────────────────

    /**
     * @brief    Subscribes a handler for events of type EventType
     *
     * The handler is appended to the per-type handler list under an exclusive
     * lock. The returned SubscriptionToken must be stored — its destructor
     * automatically unsubscribes.
     *
     * @tparam   EventType  The event struct type to subscribe to.
     * @param[in] handler   Callable with signature `void(const EventType&)`.
     *
     * @return   `[[nodiscard]]` SubscriptionToken that unsubscribes on destruction.
     */
    template <typename EventType>
    [[nodiscard]] SubscriptionToken Subscribe(std::function<void(const EventType&)> handler) {
        auto wrapped = [h = std::move(handler)](const void* evt) {
            h(*static_cast<const EventType*>(evt));
        };
        return SubscribeImpl(std::type_index(typeid(EventType)), std::move(wrapped));
    }

    // ── Dispatch ──────────────────────────────────────────────────────────

    /**
     * @brief    Dispatches an event synchronously on the calling thread
     *
     * Acquires a shared lock on the handler list, copies it, releases the lock,
     * then invokes all handlers in registration order. Handlers may themselves
     * call Subscribe() or Dispatch() without deadlocking.
     *
     * @tparam   EventType  The event struct type to dispatch.
     * @param[in] event     The event value passed by const-ref to each handler.
     */
    template <typename EventType>
    void Dispatch(const EventType& event) {
        DispatchImpl(std::type_index(typeid(EventType)), &event);
    }

    // ── DispatchAsync ─────────────────────────────────────────────────────

    /**
     * @brief    Dispatches an event asynchronously on the internal worker thread
     *
     * The event is copied immediately and the dispatch is posted to the
     * internal BackgroundWorker. Returns as soon as the task is enqueued.
     * Handlers run on the worker thread and must be thread-safe.
     *
     * @tparam   EventType  The event struct type to dispatch.
     * @param[in] event     The event value. A copy is captured in the posted task.
     */
    template <typename EventType>
    void DispatchAsync(EventType event) {
        auto eventCopy = std::make_shared<EventType>(std::move(event));
        PostAsync([this, eventCopy]() {
            Dispatch(*eventCopy);
        });
    }

private:
    struct Impl;
    std::shared_ptr<Impl> _impl;

    // Non-template helpers (defined in EventBus.cpp where Impl is fully visible)
    [[nodiscard]] SubscriptionToken SubscribeImpl(std::type_index type,
                                                  std::function<void(const void*)> handler);
    void DispatchImpl(std::type_index type, const void* event);
    void PostAsync(std::function<void()> task);
};

} // namespace ImFrame::Utility
