/**
 * @file     BackgroundWorker_test.cpp
 * @brief    Unit and thread-safety tests for ImFrame::Utility::BackgroundWorker
 *
 * @internal
 * Tests verify: FIFO task ordering, Drain() blocking until idle, PostAwaitable()
 * return values, and safe concurrent Post() from multiple threads. Labeled
 * [unit] and [tsan] so they run in both the standard and ThreadSanitizer passes.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-05-31
 * @version  0.4.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Utility/BackgroundWorker.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace ImFrame::Utility;

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("BackgroundWorker::Name returns the name passed at construction", "[unit]") {
    BackgroundWorker worker{"NameTest"};
    REQUIRE(worker.Name() == "NameTest");
}

TEST_CASE("BackgroundWorker tasks execute in FIFO submission order", "[unit][tsan]") {
    BackgroundWorker worker{"OrderWorker"};

    std::vector<int>  order;
    std::mutex        orderMtx;

    for (int i = 0; i < 50; ++i) {
        worker.Post([i, &order, &orderMtx] {
            std::lock_guard lock{orderMtx};
            order.push_back(i);
        });
    }

    worker.Drain();

    REQUIRE(order.size() == 50);
    for (int i = 0; i < 50; ++i)
        REQUIRE(order[i] == i);
}

TEST_CASE("BackgroundWorker::Drain blocks until queue is empty and worker is idle", "[unit][tsan]") {
    BackgroundWorker worker{"DrainWorker"};
    std::atomic<int> counter{0};

    for (int i = 0; i < 100; ++i)
        worker.Post([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        });

    worker.Drain();
    REQUIRE(counter.load(std::memory_order_acquire) == 100);
}

TEST_CASE("BackgroundWorker::PostAwaitable returns correct integer results", "[unit]") {
    BackgroundWorker worker{"AwaitableWorker"};

    std::vector<std::future<int>> futures;
    futures.reserve(20);
    for (int i = 0; i < 20; ++i)
        futures.push_back(worker.PostAwaitable([i] { return i * 2; }));

    for (int i = 0; i < 20; ++i)
        REQUIRE(futures[i].get() == i * 2);
}

TEST_CASE("BackgroundWorker::PostAwaitable handles void callables", "[unit]") {
    BackgroundWorker worker{"VoidAwaitWorker"};
    std::atomic<int> counter{0};

    auto f = worker.PostAwaitable([&counter] {
        counter.fetch_add(1, std::memory_order_relaxed);
    });

    f.get();
    REQUIRE(counter.load() == 1);
}

TEST_CASE("BackgroundWorker::Post from 4 concurrent threads is safe", "[tsan]") {
    BackgroundWorker       worker{"ConcurrentPostWorker"};
    std::atomic<int>       counter{0};

    static constexpr int THREADS   = 4;
    static constexpr int EACH      = 250; // 4 × 250 = 1000

    std::vector<std::jthread> producers;
    producers.reserve(THREADS);

    for (int t = 0; t < THREADS; ++t) {
        producers.emplace_back([&worker, &counter] {
            for (int i = 0; i < EACH; ++i)
                worker.Post([&counter] {
                    counter.fetch_add(1, std::memory_order_relaxed);
                });
        });
    }

    // Join producers before draining to ensure all Posts have been issued.
    producers.clear();

    worker.Drain();
    REQUIRE(counter.load(std::memory_order_acquire) == THREADS * EACH);
}

TEST_CASE("BackgroundWorker destructor drains cleanly without deadlock", "[unit][tsan]") {
    std::atomic<int> counter{0};
    {
        BackgroundWorker worker{"DtorWorker"};
        for (int i = 0; i < 30; ++i)
            worker.Post([&counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
    } // Drain() + join via dtor
    // All tasks must have run (dtor calls Drain()).
    REQUIRE(counter.load(std::memory_order_acquire) == 30);
}
