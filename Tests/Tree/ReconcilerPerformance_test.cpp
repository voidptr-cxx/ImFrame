/**
 * @file     ReconcilerPerformance_test.cpp
 * @brief    Adversarial performance/stress audit of the Phase 27 reconciler (Phase 30.5)
 *
 * Profiles `Internal::ReconcileChildren`/`ReconcileChild` (`ElementInternal.cpp`)
 * and `Internal::ComponentElement<T>`'s dirty-flag coalescing (`Widget.hpp`)
 * against the four adversarial cases and the long-running memory-growth case
 * from `PHASE_30_PROPOSAL.md`'s "Reconciler Performance Audit" section. This
 * file adds regression coverage; it does not change reconciler behaviour —
 * see `.claude/DECISIONS.md` for the audit's findings (no O(n^2) case was
 * found; the algorithm was already correct, so this phase is test-only).
 *
 * Reuses the `Counters`/instrumented-`Element` idiom already established by
 * `Reconciler_test.cpp`'s `CountingWidget` (each `Tests/Tree/*.cpp` file
 * defines its own copy in an anonymous namespace, rather than sharing one
 * across files — matching this codebase's existing convention).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-14
 * @version  2.8.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Tree/Primitives/Text.hpp"
#include "ImFrame/Tree/State.hpp"
#include "ImFrame/Tree/Widget.hpp"
#include "ImFrame/Tree/WidgetTestDriver.hpp"
#include "Tree/ElementInternal.hpp"

#include <chrono>
#include <functional>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Tree;

namespace {

struct Counters {
    int created   = 0;
    int mounted   = 0;
    int updated   = 0;
    int unmounted = 0;
};

/// Instrumented primitive A — tracks Mount/Update/Unmount counts, holds an ordered child list.
class CountingWidget {
public:
    explicit CountingWidget(Counters& counters) : _counters(&counters) {}

    CountingWidget& Children(std::vector<Widget> kids) { _children = std::move(kids); return *this; }
    CountingWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] Counters& GetCounters() const noexcept { return *_counters; }
    [[nodiscard]] const std::vector<Widget>& GetChildren() const noexcept { return _children; }

    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    Counters*            _counters;
    std::vector<Widget>  _children;
    Tree::Key             _key;
};

class CountingElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _counters = &widget.As<CountingWidget>().GetCounters();
        _counters->mounted++;
        Sync(widget);
    }

    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _counters->updated++;
        Sync(newWidget);
    }

    void Unmount() override {
        _counters->unmounted++;
        for (auto& child : _children) { child->Unmount(); }
        _children.clear();
    }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        for (auto& child : _children) { std::ignore = child->Layout(constraints); }
        return _size = {0.0f, 0.0f};
    }

    void Paint(Widgets::Vec2 position) override {
        for (auto& child : _children) { child->Paint(position); }
    }

private:
    void Sync(const Widget& widget) {
        Internal::ReconcileChildren(this, _children, widget.As<CountingWidget>().GetChildren());
    }

    Counters*                              _counters = nullptr;
    std::vector<std::unique_ptr<Element>>  _children;
};

std::unique_ptr<Element> CountingWidget::CreateElement() const {
    _counters->created++;
    return std::make_unique<CountingElement>();
}

/// Instrumented primitive B — a distinct concrete type so `CanUpdate()` always fails
/// against `CountingWidget`, forcing a genuine destroy+recreate at a shared position.
class AltCountingWidget {
public:
    explicit AltCountingWidget(Counters& counters) : _counters(&counters) {}

    [[nodiscard]] Tree::Key GetKey() const noexcept { return {}; }
    [[nodiscard]] Counters& GetCounters() const noexcept { return *_counters; }
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    Counters* _counters;
};

class AltCountingElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _counters = &widget.As<AltCountingWidget>().GetCounters();
        _counters->mounted++;
    }
    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _counters->updated++;
    }
    void Unmount() override { _counters->unmounted++; }
    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints) override { return {0.0f, 0.0f}; }
    void Paint(Widgets::Vec2) override {}

private:
    Counters* _counters = nullptr;
};

std::unique_ptr<Element> AltCountingWidget::CreateElement() const {
    _counters->created++;
    return std::make_unique<AltCountingElement>();
}

} // namespace

// ─── Deeply nested trees (100 levels) ──────────────────────────────────────────

TEST_CASE("ReconcilerPerformance: 100-level deep nesting mounts/updates/unmounts without stack overflow",
          "[unit][reconciler-perf]") {
    Counters counters;
    constexpr int kDepth = 100;

    std::function<Widget(int)> buildChain = [&](int depth) -> Widget {
        if (depth == 0) { return CountingWidget(counters); }
        return CountingWidget(counters).Children({buildChain(depth - 1)});
    };

    Widget root    = buildChain(kDepth);
    auto    element = root.CreateElement();
    REQUIRE_NOTHROW(element->Mount(nullptr, 0, root));
    REQUIRE(counters.created == kDepth + 1);

    Widget root2 = buildChain(kDepth);
    REQUIRE_NOTHROW(element->Update(root2));
    REQUIRE(counters.created == kDepth + 1); // fully reused, no new elements

    REQUIRE_NOTHROW(element->Layout(BoxConstraints::Loose({800.0f, 600.0f})));
    REQUIRE_NOTHROW(element->Paint({0.0f, 0.0f}));
    REQUIRE_NOTHROW(element->Unmount());
    REQUIRE(counters.unmounted == kDepth + 1);
}

// ─── Large flat lists (10,000 items, no VirtualList) ───────────────────────────

TEST_CASE("ReconcilerPerformance: large flat list reconciliation scales linearly, not quadratically",
          "[unit][reconciler-perf]") {
    auto buildKeyedList = [](Counters& counters, std::size_t n, bool reversed) {
        std::vector<Widget> kids;
        kids.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            std::size_t key = reversed ? (n - 1 - i) : i;
            kids.push_back(CountingWidget(counters).Key(key));
        }
        return CountingWidget(counters).Children(std::move(kids));
    };

    auto timeReconcile = [&](std::size_t n) -> double {
        Counters counters;
        Widget    root1    = buildKeyedList(counters, n, /*reversed=*/false);
        auto      element  = root1.CreateElement();
        element->Mount(nullptr, 0, root1);
        REQUIRE(counters.created == static_cast<int>(n) + 1);

        // Fully reversed key order on the next frame — worst case for the
        // unkeyed-cursor path is avoided (these are all keyed), but this still
        // forces every slot's `result` position to differ from its previous
        // index, exercising the full `unordered_map` keyed lookup path.
        Widget root2 = buildKeyedList(counters, n, /*reversed=*/true);

        const auto start = std::chrono::steady_clock::now();
        element->Update(root2);
        const auto end = std::chrono::steady_clock::now();

        REQUIRE(counters.created == static_cast<int>(n) + 1); // fully reused, zero new elements
        element->Unmount();
        return std::chrono::duration<double, std::milli>(end - start).count();
    };

    // Absolute ceiling: catches a true O(n^2) blowup by orders of magnitude
    // (10,000^2 keyed lookups would be seconds, not milliseconds) without
    // being sensitive to normal machine-to-machine timing noise.
    const double smallMs = timeReconcile(2'500);
    const double largeMs = timeReconcile(10'000);
    INFO("2,500 items: " << smallMs << "ms, 10,000 items: " << largeMs << "ms");

    REQUIRE(largeMs < 500.0);
    // O(n) would scale ~4x for a 4x input; O(n^2) would scale ~16x. Allow a
    // generous margin for scheduler/allocator noise at these small absolute
    // durations while still failing on a genuine quadratic regression.
    REQUIRE(largeMs < smallMs * 10.0 + 50.0);
}

// ─── Frequent type changes at one position ─────────────────────────────────────

TEST_CASE("ReconcilerPerformance: alternating widget types at one position leaks no elements",
          "[unit][reconciler-perf]") {
    Counters countersA;
    Counters countersB;

    std::unique_ptr<Element> child;
    constexpr int             kFrames = 2000;

    for (int frame = 0; frame < kFrames; ++frame) {
        if (frame % 2 == 0) {
            Widget w = CountingWidget(countersA);
            Internal::ReconcileChild(nullptr, child, &w);
        } else {
            Widget w = AltCountingWidget(countersB);
            Internal::ReconcileChild(nullptr, child, &w);
        }
    }
    child->Unmount();

    // Every alternation is a genuine type change (CanUpdate() false), so each
    // frame destroys the previous slot's element and creates a new one — the
    // invariant "created == unmounted" (once the final still-alive element is
    // also unmounted above) proves nothing accumulated across 2,000 swaps.
    REQUIRE(countersA.created == countersA.unmounted);
    REQUIRE(countersB.created == countersB.unmounted);
    REQUIRE(countersA.created + countersB.created == kFrames);
}

// ─── Rapid state updates (1000 SetState calls per frame) ───────────────────────

namespace {

struct Counter {
    int value = 0;
};

/// A stateful component whose `Build()` call count is externally observable,
/// so the test can assert how many *rebuilds* actually happened.
struct RebuildCounter {
    int*             buildCalls;
    State<Counter>   state;

    [[nodiscard]] Widget Build() const {
        ++(*buildCalls);
        return Primitives::Text(std::to_string(state.Get().value));
    }
};

} // namespace

TEST_CASE("ReconcilerPerformance: 1000 State::Set() calls in one frame coalesce into a single rebuild",
          "[unit][reconciler-perf]") {
    int buildCalls = 0;
    WidgetTestDriver driver{RebuildCounter{&buildCalls, {}}};

    driver.Build(); // mount -> first Build(), also registers the dirty callback via state.Get()
    REQUIRE(buildCalls == 1);
    REQUIRE(driver.Component().state.Get().value == 0);

    for (int i = 0; i < 1000; ++i) {
        driver.Component().state.Set([](Counter& c) { ++c.value; });
    }
    // buildCalls is still 1 here: Set() only flips the atomic dirty flag (idempotently),
    // it never rebuilds synchronously — rebuilding happens on the next Build()/Update().
    REQUIRE(buildCalls == 1);

    driver.Build(); // one Update() cycle: dirty flag was set (at least once), so exactly one rebuild
    REQUIRE(buildCalls == 2);
    REQUIRE(driver.Component().state.Get().value == 1000); // every mutation was still applied
}

// ─── Long-running structural churn (10,000 frames) ─────────────────────────────

TEST_CASE("ReconcilerPerformance: 10,000 frames of continuous structural churn do not leak elements",
          "[unit][reconciler-perf]") {
    Counters countersA;
    Counters countersB;
    constexpr std::size_t kLiveSlots = 50;
    constexpr int          kFrames    = 10'000;

    std::vector<Widget> initial;
    initial.reserve(kLiveSlots);
    for (std::size_t i = 0; i < kLiveSlots; ++i) { initial.push_back(CountingWidget(countersA).Key(i)); }
    Widget root      = CountingWidget(countersA).Children(std::move(initial));
    auto    element   = root.CreateElement();
    element->Mount(nullptr, 0, root);

    for (int frame = 0; frame < kFrames; ++frame) {
        std::vector<Widget> kids;
        kids.reserve(kLiveSlots);
        for (std::size_t i = 0; i < kLiveSlots; ++i) {
            // Every 3rd frame, every 7th slot swaps to a different concrete
            // type at the same key — forces a genuine destroy+recreate
            // interleaved with ordinary keyed reuse, repeatedly, at scale.
            if (frame % 3 == 0 && i % 7 == 0) {
                kids.push_back(AltCountingWidget(countersB));
            } else {
                kids.push_back(CountingWidget(countersA).Key(i));
            }
        }
        Widget next = CountingWidget(countersA).Children(std::move(kids));
        element->Update(next);
    }
    element->Unmount();

    // If reconciliation leaked a single element per churned frame, `created`
    // would grow to roughly kFrames * kLiveSlots; instead it should track
    // `unmounted` almost exactly (every created element is eventually
    // unmounted, either by churn during the loop or by the final Unmount()).
    REQUIRE(countersA.created == countersA.unmounted);
    REQUIRE(countersB.created == countersB.unmounted);
}
