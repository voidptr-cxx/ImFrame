/**
 * @file     EventBus.cpp
 * @brief    EventBus and SubscriptionToken implementation
 *
 * @internal
 * EventBus::Impl holds one HandlerList per event type_index. Each HandlerList
 * is protected by its own std::shared_mutex:
 *   - Subscribe / Unsubscribe: exclusive lock on the per-type list
 *   - Dispatch: shared lock to copy the list, then no lock while invoking
 *
 * The map from type_index → HandlerList is protected by a std::shared_mutex:
 *   - Read (existing type look-up): shared lock
 *   - Write (new type insertion):   exclusive lock
 *
 * DispatchAsync posts a lambda to the internal BackgroundWorker. The lambda
 * captures a shared_ptr to the event copy and calls Dispatch<E>() on the
 * worker thread.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-01
 * @version  0.5.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "ImFrame/Utility/EventBus.hpp"
#include "ImFrame/Utility/BackgroundWorker.hpp"

#include <algorithm>
#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace ImFrame::Utility {

// ─── EventBus::Impl ──────────────────────────────────────────────────────────

struct EventBus::Impl {
    // Per-type handler entry
    struct HandlerEntry {
        std::uint64_t id;
        std::function<void(const void*)> fn;
    };

    // Per-type handler list, guarded by its own shared_mutex
    struct HandlerList {
        mutable std::shared_mutex mutex;
        std::vector<HandlerEntry> entries;
        std::uint64_t nextId{0};
    };

    // Map from event type_index → handler list (not copyable due to shared_mutex)
    mutable std::shared_mutex mapMutex;
    std::unordered_map<std::type_index, std::unique_ptr<HandlerList>> map;

    // Single background worker for async dispatch
    BackgroundWorker asyncWorker{"EventBus-Async"};

    // ── Map helpers ───────────────────────────────────────────────────────

    HandlerList& GetOrCreate(std::type_index type) {
        // Fast path: type already registered
        {
            std::shared_lock rlock{mapMutex};
            auto it = map.find(type);
            if (it != map.end()) return *it->second;
        }
        // Slow path: first subscription for this type
        std::unique_lock wlock{mapMutex};
        auto& ptr = map[type];
        if (!ptr) ptr = std::make_unique<HandlerList>();
        return *ptr;
    }

    HandlerList* Get(std::type_index type) const {
        std::shared_lock rlock{mapMutex};
        auto it = map.find(type);
        return (it != map.end()) ? it->second.get() : nullptr;
    }

    // ── Subscribe / Unsubscribe ───────────────────────────────────────────

    std::uint64_t Subscribe(std::type_index type, std::function<void(const void*)> fn) {
        HandlerList& list = GetOrCreate(type);
        std::unique_lock lock{list.mutex};
        auto id = list.nextId++;
        list.entries.push_back({id, std::move(fn)});
        return id;
    }

    void Unsubscribe(std::type_index type, std::uint64_t id) noexcept {
        HandlerList* list = Get(type);
        if (!list) return;
        std::unique_lock lock{list->mutex};
        auto& entries = list->entries;
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [id](const HandlerEntry& e) { return e.id == id; }),
            entries.end());
    }

    // ── Dispatch ──────────────────────────────────────────────────────────

    void Dispatch(std::type_index type, const void* evt) {
        HandlerList* list = Get(type);
        if (!list) return;

        // Copy under shared lock, then invoke without holding the lock
        std::vector<HandlerEntry> copy;
        {
            std::shared_lock lock{list->mutex};
            copy = list->entries;
        }
        for (auto& entry : copy) entry.fn(evt);
    }
};

// ─── SubscriptionToken ────────────────────────────────────────────────────────

SubscriptionToken::SubscriptionToken(std::function<void()> unsubscribeFn)
    : _unsubscribeFn{std::move(unsubscribeFn)} {}

SubscriptionToken::~SubscriptionToken() noexcept {
    Unsubscribe();
}

SubscriptionToken::SubscriptionToken(SubscriptionToken&& other) noexcept
    : _unsubscribeFn{std::move(other._unsubscribeFn)} {
    other._unsubscribeFn = nullptr;
}

SubscriptionToken& SubscriptionToken::operator=(SubscriptionToken&& other) noexcept {
    if (this != &other) {
        Unsubscribe();
        _unsubscribeFn       = std::move(other._unsubscribeFn);
        other._unsubscribeFn = nullptr;
    }
    return *this;
}

void SubscriptionToken::Unsubscribe() noexcept {
    if (_unsubscribeFn) {
        _unsubscribeFn();
        _unsubscribeFn = nullptr;
    }
}

bool SubscriptionToken::IsSubscribed() const noexcept {
    return static_cast<bool>(_unsubscribeFn);
}

// ─── EventBus ─────────────────────────────────────────────────────────────────

EventBus::EventBus()
    : _impl{std::make_shared<Impl>()} {}

EventBus::~EventBus() noexcept = default;

EventBus& EventBus::Instance() {
    static EventBus singleton;
    return singleton;
}

SubscriptionToken EventBus::SubscribeImpl(std::type_index type,
                                          std::function<void(const void*)> handler) {
    auto id = _impl->Subscribe(type, std::move(handler));

    // Capture weak_ptr<Impl> so the token's unsubscribe is safe after bus destruction
    std::weak_ptr<Impl> weak{_impl};
    return SubscriptionToken{[weak, type, id]() noexcept {
        if (auto impl = weak.lock()) {
            impl->Unsubscribe(type, id);
        }
    }};
}

void EventBus::DispatchImpl(std::type_index type, const void* event) {
    _impl->Dispatch(type, event);
}

void EventBus::PostAsync(std::function<void()> task) {
    _impl->asyncWorker.Post(std::move(task));
}

} // namespace ImFrame::Utility
