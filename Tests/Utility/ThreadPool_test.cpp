/**
 * @file     ThreadPool_test.cpp
 * @brief    Unit and thread-safety tests for ImFrame::Utility::ThreadPool
 *
 * @internal
 * Tests verify: Submit() return values, WaitAll() completion, SubmitBatch()
 * correctness, and concurrent submission under ThreadSanitizer. The stress
 * test (8 producers × 10 000 tasks) is labeled [tsan] for the sanitizer pass.
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

#include "ImFrame/Utility/ThreadPool.hpp"

#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

using namespace ImFrame::Utility;

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("ThreadPool::Submit returns correct values for 1000 tasks", "[unit]") {
    ThreadPool pool{4, "UnitPool"};

    std::vector<std::future<int>> futures;
    futures.reserve(1000);
    for (int i = 0; i < 1000; ++i)
        futures.push_back(pool.Submit([i] { return i * i; }));

    for (int i = 0; i < 1000; ++i)
        REQUIRE(futures[i].get() == i * i);
}

TEST_CASE("ThreadPool::WaitAll returns after all submitted tasks complete", "[unit]") {
    ThreadPool pool{4, "WaitAllPool"};
    std::atomic<int> counter{0};

    for (int i = 0; i < 500; ++i)
        pool.Submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });

    pool.WaitAll();
    REQUIRE(counter.load(std::memory_order_acquire) == 500);
}

TEST_CASE("ThreadPool::SubmitBatch submits a range and returns correct futures", "[unit]") {
    ThreadPool pool{4, "BatchPool"};

    std::vector<std::function<int()>> tasks;
    tasks.reserve(100);
    for (int i = 0; i < 100; ++i)
        tasks.emplace_back([i] { return i + 1; });

    auto futures = pool.SubmitBatch(tasks);
    REQUIRE(futures.size() == 100);

    for (int i = 0; i < 100; ++i)
        REQUIRE(futures[i].get() == i + 1);
}

TEST_CASE("ThreadPool::Submit handles void callables", "[unit]") {
    ThreadPool pool{2, "VoidPool"};
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 50; ++i)
        futures.push_back(pool.Submit([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));

    for (auto& f : futures)
        f.get();

    REQUIRE(counter.load() == 50);
}

TEST_CASE("ThreadPool WorkerCount returns the count passed at construction", "[unit]") {
    ThreadPool pool{6, "CountPool"};
    REQUIRE(pool.WorkerCount() == 6);
}

TEST_CASE("ThreadPool stress: 8 producers submit 10000 tasks concurrently", "[tsan]") {
    ThreadPool pool{8, "StressPool"};
    std::atomic<int> totalCompleted{0};

    static constexpr int PRODUCERS     = 8;
    static constexpr int TASKS_EACH    = 1250; // 8 × 1250 = 10 000

    std::vector<std::jthread> producers;
    producers.reserve(PRODUCERS);

    for (int p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&pool, &totalCompleted] {
            for (int i = 0; i < TASKS_EACH; ++i) {
                pool.Submit([&totalCompleted] {
                    totalCompleted.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }

    // Let all producers finish submitting.
    producers.clear(); // jthread dtors join

    pool.WaitAll();
    REQUIRE(totalCompleted.load(std::memory_order_acquire) == PRODUCERS * TASKS_EACH);
}

TEST_CASE("ThreadPool destructor shuts down cleanly without deadlock", "[unit][tsan]") {
    std::atomic<int> counter{0};
    {
        ThreadPool pool{2, "DtorPool"};
        for (int i = 0; i < 20; ++i)
            pool.Submit([&counter] {
                counter.fetch_add(1, std::memory_order_relaxed);
            });
    } // Shutdown() + join via dtor
    // All 20 tasks may or may not have run — we just check there's no deadlock.
    REQUIRE(counter.load() >= 0);
}
