/**
 * @file     Reconciler_test.cpp
 * @brief    Unit tests for Element reconciliation (Mount/Update/Unmount) and key-based identity
 *
 * Uses a custom test-local `CountingWidget` primitive (satisfies
 * `Tree::PrimitiveWidget`) whose `Element` does no ImGui work — these tests
 * exercise reconciliation directly via `Widget`/`Element`, without an ImGui
 * context, `Application`, or `Reconciler::Show()` (which does touch ImGui via
 * `RootBridge`).
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-30
 * @version  2.2.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <catch2/catch_test_macros.hpp>

#include "ImFrame/Tree/Widget.hpp"
#include "Tree/ElementInternal.hpp"

#include <functional>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Tree;

namespace {

struct Counters {
    int                              created   = 0;
    int                              mounted   = 0;
    int                              updated   = 0;
    int                              unmounted = 0;
    std::unordered_map<int, void*>   liveByTag;
};

class CountingWidget {
public:
    explicit CountingWidget(Counters& counters, int tag = 0) : _counters(&counters), _tag(tag) {}

    CountingWidget& Children(std::vector<Widget> kids) { _children = std::move(kids); return *this; }
    CountingWidget& Key(std::uint64_t k) noexcept { _key = Tree::Key(k); return *this; }

    [[nodiscard]] Tree::Key GetKey() const noexcept { return _key; }
    [[nodiscard]] Counters& GetCounters() const noexcept { return *_counters; }
    [[nodiscard]] int       GetTag() const noexcept { return _tag; }
    [[nodiscard]] const std::vector<Widget>& GetChildren() const noexcept { return _children; }

    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    Counters*            _counters;
    int                   _tag = 0;
    std::vector<Widget>  _children;
    Tree::Key             _key;
};

class CountingElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        const CountingWidget& w = widget.As<CountingWidget>();
        _counters = &w.GetCounters();
        _tag      = w.GetTag();
        _counters->mounted++;
        _counters->liveByTag[_tag] = this;
        Sync(widget);
    }

    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        const CountingWidget& w = newWidget.As<CountingWidget>();
        _tag                     = w.GetTag();
        _counters->updated++;
        _counters->liveByTag[_tag] = this;
        Sync(newWidget);
    }

    void Unmount() override {
        _counters->unmounted++;
        _counters->liveByTag.erase(_tag);
        for (auto& child : _children) { child->Unmount(); }
        _children.clear();
    }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        for (auto& child : _children) { std::ignore = child->Layout(constraints); }
        _size = {0.0f, 0.0f};
        return _size;
    }

    void Paint(Rendering::CommandBuffer& cmd, Widgets::Vec2 position) override {
        for (auto& child : _children) { child->Paint(cmd, position); }
    }

private:
    void Sync(const Widget& widget) {
        const CountingWidget& w = widget.As<CountingWidget>();
        Internal::ReconcileChildren(this, _children, w.GetChildren());
    }

    Counters*                              _counters = nullptr;
    int                                     _tag      = 0;
    std::vector<std::unique_ptr<Element>>  _children;
};

std::unique_ptr<Element> CountingWidget::CreateElement() const {
    _counters->created++;
    return std::make_unique<CountingElement>();
}

} // namespace

// ─── Stable tree ──────────────────────────────────────────────────────────────

TEST_CASE("Reconciler: stable tree reuses elements across Update (no new creates)", "[unit]") {
    Counters counters;

    Widget root1 = CountingWidget(counters, 1).Children({
        CountingWidget(counters, 2),
        CountingWidget(counters, 3),
    });
    auto element = root1.CreateElement();
    element->Mount(nullptr, 0, root1);
    REQUIRE(counters.created == 3);
    REQUIRE(counters.mounted == 3);

    Widget root2 = CountingWidget(counters, 1).Children({
        CountingWidget(counters, 2),
        CountingWidget(counters, 3),
    });
    REQUIRE(element->CanUpdate(root2));
    element->Update(root2);

    REQUIRE(counters.created == 3);   // unchanged — no new elements
    REQUIRE(counters.updated >= 3);   // root + both children updated
    element->Unmount();
}

// ─── Type change ──────────────────────────────────────────────────────────────

struct OtherWidget {
    [[nodiscard]] Tree::Key GetKey() const noexcept { return Tree::Key{}; }
    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;
};

class OtherElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent = parent; _slotIndex = slotIndex; RecordWidgetMeta(widget);
    }
    void Update(const Widget& newWidget) override { RecordWidgetMeta(newWidget); }
    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints) override { return {0.0f, 0.0f}; }
    void Paint(Rendering::CommandBuffer&, Widgets::Vec2) override {}
};

std::unique_ptr<Element> OtherWidget::CreateElement() const { return std::make_unique<OtherElement>(); }

TEST_CASE("Reconciler: type change at a position destroys the old element and creates a new one", "[unit]") {
    Counters counters;

    Widget w1 = CountingWidget(counters, 1);
    auto   element = w1.CreateElement();
    element->Mount(nullptr, 0, w1);
    REQUIRE(counters.created == 1);

    Widget w2 = OtherWidget{};
    REQUIRE_FALSE(element->CanUpdate(w2));

    element->Unmount();
    REQUIRE(counters.unmounted == 1);

    element = w2.CreateElement();
    element->Mount(nullptr, 0, w2);
    // No crash / no counter assertions needed for OtherElement — type swap succeeded.
}

// ─── Key-based reorder ─────────────────────────────────────────────────────────

TEST_CASE("Reconciler: key-based reorder preserves element identity", "[unit]") {
    Counters counters;

    Widget root1 = CountingWidget(counters, 100).Children({
        CountingWidget(counters, 1).Key(1),
        CountingWidget(counters, 2).Key(2),
    });
    auto element = root1.CreateElement();
    element->Mount(nullptr, 0, root1);
    REQUIRE(counters.created == 3);

    void* aBefore = counters.liveByTag.at(1);
    void* bBefore = counters.liveByTag.at(2);

    // Reordered: B first, then A — same keys.
    Widget root2 = CountingWidget(counters, 100).Children({
        CountingWidget(counters, 2).Key(2),
        CountingWidget(counters, 1).Key(1),
    });
    element->Update(root2);

    REQUIRE(counters.created == 3); // no new elements created by the reorder
    REQUIRE(counters.liveByTag.at(1) == aBefore);
    REQUIRE(counters.liveByTag.at(2) == bBefore);

    element->Unmount();
}

// ─── Deep nesting ──────────────────────────────────────────────────────────────

TEST_CASE("Reconciler: 50-level deep nesting reconciles without stack overflow", "[unit]") {
    Counters counters;

    constexpr int kDepth = 50;

    std::function<Widget(int)> buildChain = [&](int depth) -> Widget {
        if (depth == 0) { return CountingWidget(counters, depth); }
        return CountingWidget(counters, depth).Children({buildChain(depth - 1)});
    };

    Widget root = buildChain(kDepth);
    auto   element = root.CreateElement();
    REQUIRE_NOTHROW(element->Mount(nullptr, 0, root));
    REQUIRE(counters.created == kDepth + 1);

    REQUIRE_NOTHROW(element->Layout(BoxConstraints::Loose({800.0f, 600.0f})));
    Rendering::CommandBuffer cmd;
    REQUIRE_NOTHROW(element->Paint(cmd, {0.0f, 0.0f}));
    REQUIRE_NOTHROW(element->Unmount());
    REQUIRE(counters.unmounted == kDepth + 1);
}
