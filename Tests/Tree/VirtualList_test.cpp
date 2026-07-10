/**
 * @file     VirtualList_test.cpp
 * @brief    Behavioral tests for `VirtualList` — visible-range virtualisation
 *
 * Drives a real `HeadlessBackend` + `Application::SetRoot()` so `VirtualList`'s
 * `Paint()` (which opens a real ImGui scrolling child region) runs against a
 * live ImGui context. Scroll position is driven from *inside* the per-index
 * `builder` `Delegate` — since `VirtualListElement::Paint()` calls the builder
 * while its own `"##vlist"` child window is the current ImGui window,
 * `ImGui::SetScrollY()`/`GetScrollMaxY()` called there operate on that window.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-05
 * @version  2.4.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Tree/Primitives/Box.hpp"
#include "ImFrame/Tree/Primitives/SizedBox.hpp"
#include "ImFrame/Tree/VirtualList.hpp"

#include <catch2/catch_test_macros.hpp>
#include <imgui.h>

#include <cmath>
#include <memory>
#include <set>

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
    return WindowConfig{.Title = "VirtualListTest", .Width = 400, .Height = 300};
}

constexpr int   kItemCount   = 10000;
constexpr float kItemHeight  = 20.0f;
constexpr float kViewportW   = 300.0f;
constexpr float kViewportH   = 150.0f;

struct VListTestState {
    std::set<int> builtIndices;
    int           totalBuilderCalls  = 0;
    int           frame              = 0;
    bool          scrolledToBottom   = false;
    float         observedScrollMaxY = -1.0f;
};

} // namespace

TEST_CASE("VirtualList: only visible items are built across a 10,000-item list", "[tree][virtuallist]") {
    VListTestState state;

    struct Root {
        VListTestState* state;
        [[nodiscard]] Widget Build() const {
            return Box().Width(kViewportW).Height(kViewportH).Child(VirtualList(
                kItemCount, kItemHeight,
                [s = state](int index) -> Widget {
                    s->builtIndices.insert(index);
                    ++s->totalBuilderCalls;
                    return SizedBox().Width(kViewportW).Height(kItemHeight);
                }));
        }
    } root{&state};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());

    // ~150/20 = 7.5 rows visible at a time; across 3 frames with no scrolling the
    // same handful of indices repeat. Far fewer than kItemCount distinct indices,
    // and far fewer total calls than kItemCount * frames, proves virtualization.
    CHECK(state.builtIndices.size() < 20);
    CHECK(state.totalBuilderCalls < 40);
}

TEST_CASE("VirtualList: scrolling to bottom builds the last items", "[tree][virtuallist]") {
    VListTestState state;

    struct Root {
        VListTestState* state;
        [[nodiscard]] Widget Build() const {
            return Box().Width(kViewportW).Height(kViewportH).Child(VirtualList(
                kItemCount, kItemHeight,
                [s = state](int index) -> Widget {
                    s->builtIndices.insert(index);
                    // ScrollMaxY reflects the *previous* frame's settled content size — it
                    // reads 0 on the window's very first frame, so wait a couple of frames
                    // before using it to scroll to the bottom.
                    if (s->frame >= 3 && !s->scrolledToBottom) {
                        ImGui::SetScrollY(ImGui::GetScrollMaxY());
                        s->scrolledToBottom = true;
                    }
                    return SizedBox().Width(kViewportW).Height(kItemHeight);
                }));
        }
    } root{&state};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(8), TestConfig());
    app.SetRoot(root);
    app.OnUpdate([&](float) { ++state.frame; });
    REQUIRE(app.Run().has_value());

    CHECK(state.builtIndices.contains(kItemCount - 1));
}

TEST_CASE("VirtualList: fixed item height produces the correct scroll extent", "[tree][virtuallist]") {
    VListTestState state;

    struct Root {
        VListTestState* state;
        [[nodiscard]] Widget Build() const {
            return Box().Width(kViewportW).Height(kViewportH).Child(VirtualList(
                kItemCount, kItemHeight,
                [s = state](int index) -> Widget {
                    // Overwritten every call; the last frame's value is the settled one —
                    // ScrollMaxY reflects the *previous* frame's committed content size.
                    s->observedScrollMaxY = ImGui::GetScrollMaxY();
                    return SizedBox().Width(kViewportW).Height(kItemHeight);
                }));
        }
    } root{&state};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(4), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());

    REQUIRE(state.observedScrollMaxY >= 0.0f);
    const float expected = (static_cast<float>(kItemCount) * kItemHeight) - kViewportH;
    CHECK(std::abs(state.observedScrollMaxY - expected) < 20.0f);
}

TEST_CASE("VirtualList: zero item count paints nothing and does not crash", "[tree][virtuallist]") {
    struct Root {
        [[nodiscard]] Widget Build() const {
            return Box().Width(kViewportW).Height(kViewportH).Child(
                VirtualList(0, kItemHeight, [](int) -> Widget { return SizedBox{}; }));
        }
    } root;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(2), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());
}
