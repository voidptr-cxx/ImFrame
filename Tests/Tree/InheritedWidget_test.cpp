/**
 * @file     InheritedWidget_test.cpp
 * @brief    Integration tests for `InheritedWidget<T>` and `Context::Of<T>()`
 *
 * Tests are divided into two tiers:
 *  - **Pure element-tree tests**: construct element trees manually, drive
 *    Mount/Update by hand — no ImGui context required.
 *  - **Reconciler-driven tests**: run a FrameLimitedHeadlessBackend +
 *    Application::SetRoot() to test full reconciler integration with
 *    InheritedWidget and dirty cascades.
 *
 * @note     The test value type is named `AppTheme` (not `Theme`) to avoid
 *           colliding with the `ImFrame::Theme::` namespace brought in scope
 *           by `using namespace ImFrame;`.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-02
 * @version  2.3.0
 *
 * @internal
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Backends/Headless/HeadlessBackend.hpp"

#include "ImFrame/App/Application.hpp"
#include "ImFrame/Tree/Context.hpp"
#include "ImFrame/Tree/InheritedWidget.hpp"
#include "ImFrame/Tree/Primitives/SizedBox.hpp"
#include "ImFrame/Tree/Signal.hpp"
#include "ImFrame/Tree/State.hpp"
#include "ImFrame/Tree/Widget.hpp"

#include <catch2/catch_test_macros.hpp>

#include <functional>
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

// ─── Test value types ─────────────────────────────────────────────────────────

struct AppTheme {
    std::string name;
    float       fontSize = 14.f;
    bool operator==(const AppTheme&) const = default;
};

struct AppLocale {
    std::string language;
    bool operator==(const AppLocale&) const = default;
};

// ─── Test component: reads Context::Of<AppTheme>() ────────────────────────────

struct ThemeReader {
    std::shared_ptr<std::string> lastSeen = std::make_shared<std::string>("(none)");
    std::shared_ptr<int>         buildCount = std::make_shared<int>(0);

    Widget Build() const {
        ++*buildCount;
        if (const AppTheme* t = Context::Of<AppTheme>()) {
            *lastSeen = t->name;
        } else {
            *lastSeen = "(none)";
        }
        return SizedBox{};
    }
};

// ─── Test component: reads inner AppTheme (should shadow outer) ───────────────

struct InnerAndOuterReader {
    std::shared_ptr<std::string> seen = std::make_shared<std::string>("(none)");

    Widget Build() const {
        if (const AppTheme* t = Context::Of<AppTheme>()) {
            *seen = t->name;
        }
        return SizedBox{};
    }
};

} // anonymous namespace

// ─── Pure element-tree tests (no ImGui) ──────────────────────────────────────

TEST_CASE("InheritedWidget<T>: Context::Of returns nullptr when no ancestor", "[tree][inherited]") {
    struct NoInheritedParent {
        std::shared_ptr<const AppTheme*> result =
            std::make_shared<const AppTheme*>(reinterpret_cast<const AppTheme*>(1));

        Widget Build() const {
            *result = Context::Of<AppTheme>();
            return SizedBox{};
        }
    };

    NoInheritedParent c;
    Widget w(c);
    auto elem = w.CreateElement();
    elem->Mount(nullptr, 0, w);
    CHECK(*c.result == nullptr);
}

TEST_CASE("InheritedWidget<T>: Context::Of finds nearest ancestor value", "[tree][inherited]") {
    ThemeReader reader;
    Widget child(reader);
    Widget root = InheritedWidget<AppTheme>{AppTheme{"Dracula"}, child};

    auto elem = root.CreateElement();
    elem->Mount(nullptr, 0, root);

    CHECK(*reader.lastSeen == "Dracula");
}

TEST_CASE("InheritedWidget<T>: inner InheritedWidget shadows outer", "[tree][inherited]") {
    InnerAndOuterReader reader;
    // Inner InheritedWidget<AppTheme> wraps reader; outer wraps inner
    Widget inner = InheritedWidget<AppTheme>{AppTheme{"Nord"}, reader};
    Widget outer = InheritedWidget<AppTheme>{AppTheme{"Dracula"}, inner};

    auto elem = outer.CreateElement();
    elem->Mount(nullptr, 0, outer);

    // reader should see "Nord" (nearest ancestor), not "Dracula"
    CHECK(*reader.seen == "Nord");
}

TEST_CASE("InheritedWidget<T>: Context::Of returns nullptr for different type T", "[tree][inherited]") {
    struct LocaleReader {
        std::shared_ptr<const AppLocale*> result =
            std::make_shared<const AppLocale*>(reinterpret_cast<const AppLocale*>(1));

        Widget Build() const {
            *result = Context::Of<AppLocale>();
            return SizedBox{};
        }
    };

    LocaleReader reader;
    // Only AppTheme is provided — AppLocale lookup must return nullptr
    Widget root = InheritedWidget<AppTheme>{AppTheme{"Light"}, reader};
    auto elem = root.CreateElement();
    elem->Mount(nullptr, 0, root);

    CHECK(*reader.result == nullptr);
}

TEST_CASE("InheritedWidget<T>: value change on Update notifies dependent consumers", "[tree][inherited]") {
    ThemeReader reader;
    // Mount with "Dracula"
    Widget v1 = InheritedWidget<AppTheme>{AppTheme{"Dracula"}, reader};
    auto elem = v1.CreateElement();
    elem->Mount(nullptr, 0, v1);
    REQUIRE(*reader.lastSeen == "Dracula");
    int buildsAfterMount = *reader.buildCount;

    // Updating with same value should NOT rebuild the consumer
    Widget v1b = InheritedWidget<AppTheme>{AppTheme{"Dracula"}, reader};
    elem->Update(v1b);
    CHECK(*reader.buildCount == buildsAfterMount);  // dirty not fired — same value

    // Updating with new value SHOULD mark consumer dirty → rebuild
    Widget v2 = InheritedWidget<AppTheme>{AppTheme{"Nord"}, reader};
    elem->Update(v2);
    CHECK(*reader.lastSeen == "Nord");
    CHECK(*reader.buildCount > buildsAfterMount);
}

TEST_CASE("InheritedWidget<T>: non-equality-comparable type always notifies on Update", "[tree][inherited]") {
    // Use a type without operator== — change is always assumed
    struct NonComparable {
        int value = 0;
        // No operator==
    };
    struct NCReader {
        std::shared_ptr<int>  seen = std::make_shared<int>(-1);
        std::shared_ptr<int>  count = std::make_shared<int>(0);

        Widget Build() const {
            ++*count;
            if (const NonComparable* nc = Context::Of<NonComparable>()) {
                *seen = nc->value;
            }
            return SizedBox{};
        }
    };

    NCReader reader;
    Widget v1 = InheritedWidget<NonComparable>{NonComparable{1}, reader};
    auto elem = v1.CreateElement();
    elem->Mount(nullptr, 0, v1);
    REQUIRE(*reader.seen == 1);
    int initialCount = *reader.count;

    // Same semantic value, but no operator== — still notifies
    Widget v2 = InheritedWidget<NonComparable>{NonComparable{1}, reader};
    elem->Update(v2);
    CHECK(*reader.count > initialCount);
}

// ─── Reconciler-driven integration tests ─────────────────────────────────────

WindowConfig TestConfig() {
    return WindowConfig{.Title = "InheritedWidgetTest", .Width = 400, .Height = 300};
}

TEST_CASE("InheritedWidget<T>: full reconciler round-trip via HeadlessBackend", "[tree][inherited][integration]") {
    struct Root {
        ThemeReader* reader;
        [[nodiscard]] Widget Build() const {
            return InheritedWidget<AppTheme>{AppTheme{"CatppuccinMocha"}, *reader};
        }
    };

    ThemeReader reader;
    Root        root{&reader};

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(3), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());

    CHECK(*reader.lastSeen == "CatppuccinMocha");
    CHECK(*reader.buildCount >= 1);
}

TEST_CASE("InheritedWidget<T>: dirty cascade via Signal change triggers consumer rebuild", "[tree][inherited][integration]") {
    // A stateful root that changes an owned Signal<AppTheme> on the second Build.
    struct Root {
        Signal<AppTheme>             theme{AppTheme{"Dracula"}};
        std::shared_ptr<ThemeReader> reader = std::make_shared<ThemeReader>();
        State<int>                   framesSeen;

        [[nodiscard]] Widget Build() const {
            const int seen = framesSeen.Get();
            framesSeen.Set([](int& v) { ++v; });
            if (seen == 1) {
                theme = AppTheme{"Nord"};
            }
            return InheritedWidget<AppTheme>{theme.Get(), *reader};
        }
    };

    Root root;
    auto readerPtr = root.reader;

    Application app(std::make_unique<FrameLimitedHeadlessBackend>(5), TestConfig());
    app.SetRoot(root);
    REQUIRE(app.Run().has_value());

    // After the theme change on frame 2, the reader must have seen "Nord".
    CHECK(*readerPtr->lastSeen == "Nord");
}
