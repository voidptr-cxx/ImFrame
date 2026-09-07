/**
 * @file     Portal_test.cpp
 * @brief    Behavioral tests for `Portal` — root-scope deferred rendering
 *
 * `HeadlessBackend`'s `ReadPixels()` is a zero-filled stub (real headless
 * rendering is Phase 36's `SoftwareRenderer`), so "z-order in the draw list"
 * is verified indirectly: a test-local `TraceWidget` primitive records its own
 * tag into a shared `TraceEvents::paintOrder` vector every time `Paint()` runs.
 * Since `Element::Paint()` call order *is* what determines ImGui draw-command
 * order (later `Paint()` calls emit their draw commands later, drawing on
 * top), checking paint-call order is equivalent to checking z-order.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx
 * @license   MIT — see LICENSE in the project root for the full text
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Tree/Portal.hpp"
#include "ImFrame/Tree/Primitives/Flex.hpp"
#include "ImFrame/Tree/Primitives/SizedBox.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace ImFrame;
using namespace ImFrame::Tree;
using namespace ImFrame::Tree::Primitives;
using ImFrame::App::Application;

namespace {

/// HeadlessBackend subclass that stops the render loop after N full frames.
class FrameLimitedHeadlessBackend final : public Internal::HeadlessBackend {
public:
    explicit FrameLimitedHeadlessBackend(int maxFrames) : _maxFrames(maxFrames) {}

    FrameInfo Poll() override {
        FrameInfo info   = Internal::HeadlessBackend::Poll();
        info.ShouldClose = (_frameCount >= _maxFrames);
        return info;
    }

    void BeginFrame(WindowHandle h = PrimaryWindow) override {
        Internal::HeadlessBackend::BeginFrame(h);
        ++_frameCount;
    }

private:
    int _maxFrames;
    int _frameCount = 0;
};

WindowConfig TestConfig() {
    return WindowConfig{.Title = "PortalTest", .Width = 400, .Height = 300};
}

// ─── TraceWidget: test-local primitive recording Paint()/Unmount() order ──────

struct TraceEvents {
    std::vector<std::string> paintOrder;
    int                      unmountCount = 0;

    [[nodiscard]] int LastIndexOf(const std::string& tag) const {
        for (int i = static_cast<int>(paintOrder.size()) - 1; i >= 0; --i) {
            if (paintOrder[i] == tag) { return i; }
        }
        return -1;
    }
};

class TraceWidget {
public:
    TraceWidget() = default;
    TraceWidget(TraceEvents& events, std::string tag) : _events(&events), _tag(std::move(tag)) {}

    [[nodiscard]] TraceEvents&        GetEvents() const noexcept { return *_events; }
    [[nodiscard]] const std::string&  GetTag() const noexcept { return _tag; }

    [[nodiscard]] std::unique_ptr<Element> CreateElement() const;

private:
    TraceEvents* _events = nullptr;
    std::string  _tag;
};

class TraceElement final : public Element {
public:
    void Mount(Element* parent, std::size_t slotIndex, const Widget& widget) override {
        _parent    = parent;
        _slotIndex = slotIndex;
        RecordWidgetMeta(widget);
        _config = widget.As<TraceWidget>();
    }

    void Update(const Widget& newWidget) override {
        RecordWidgetMeta(newWidget);
        _config = newWidget.As<TraceWidget>();
    }

    void Unmount() override { _config.GetEvents().unmountCount++; }

    [[nodiscard]] Widgets::Vec2 Layout(BoxConstraints constraints) override {
        _size = constraints.Constrain({10.0f, 10.0f});
        return _size;
    }

    void Paint(Rendering::CommandBuffer& /*cmd*/, Widgets::Vec2 /*position*/) override {
        _config.GetEvents().paintOrder.push_back(_config.GetTag());
    }

private:
    TraceWidget _config;
};

std::unique_ptr<Element> TraceWidget::CreateElement() const { return std::make_unique<TraceElement>(); }

Widget Trace(TraceEvents& events, std::string tag) { return TraceWidget(events, std::move(tag)); }

} // namespace

// ─── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("Portal: child renders after main tree content regardless of declaration order", "[tree][portal]") {
    TraceEvents events;

    struct Root {
        TraceEvents* events;
        [[nodiscard]] Widget Build() const {
            return Flex(Flex::Axis::Vertical)
                .Children({
                    Trace(*events, "before-portal"),
                    Portal(Trace(*events, "portal-child")),
                    Trace(*events, "after-portal"),
                });
        }
    } root{&events};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());

    const int beforeIdx = events.LastIndexOf("before-portal");
    const int afterIdx   = events.LastIndexOf("after-portal");
    const int portalIdx  = events.LastIndexOf("portal-child");

    REQUIRE(beforeIdx >= 0);
    REQUIRE(afterIdx >= 0);
    REQUIRE(portalIdx >= 0);

    CHECK(afterIdx > beforeIdx);   // sanity: normal tree order preserved
    CHECK(portalIdx > afterIdx);   // portal content paints last, even though declared before "after-portal"
}

TEST_CASE("Portal: two portals render in registration order", "[tree][portal]") {
    TraceEvents events;

    struct Root {
        TraceEvents* events;
        [[nodiscard]] Widget Build() const {
            return Flex(Flex::Axis::Vertical)
                .Children({
                    Portal(Trace(*events, "portalA")),
                    Portal(Trace(*events, "portalB")),
                });
        }
    } root{&events};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());

    const int idxA = events.LastIndexOf("portalA");
    const int idxB = events.LastIndexOf("portalB");
    REQUIRE(idxA >= 0);
    REQUIRE(idxB >= 0);
    CHECK(idxA < idxB);
}

TEST_CASE("Portal: child is unmounted when its structural parent unmounts", "[tree][portal]") {
    TraceEvents events;
    bool        showPortal = true;
    int         frame      = 0;

    struct Root {
        TraceEvents* events;
        bool*        showPortal;
        [[nodiscard]] Widget Build() const {
            if (*showPortal) { return Portal(Trace(*events, "conditional")); }
            return SizedBox{};
        }
    } root{&events, &showPortal};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(4), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) {
        ++frame;
        if (frame == 2) { showPortal = false; }
    });
    REQUIRE(app.Run().has_value());

    CHECK(events.unmountCount >= 1);
    // Painted exactly once (the single frame before it was hidden) — proves it
    // stopped repainting on every subsequent frame, not just that it's absent
    // from the tail (which would trivially hold since SizedBox paints nothing).
    const auto conditionalPaints =
        std::count(events.paintOrder.begin(), events.paintOrder.end(), std::string("conditional"));
    CHECK(conditionalPaints == 1);
}
