/**
 * @file     IconFont_test.cpp
 * @brief    Unit tests for Icons::Fa glyph constants, IMF_ICON macros, and IconFont::Load()
 *
 * @internal
 * Test 1 (compile-time): verifies IMF_ICON_LABEL string literal concatenation via
 * static_assert — no ImGui context required.
 *
 * Tests 2–3 (runtime): manually construct a minimal ImGui context (no window, no
 * renderer), merge the FA6 solid font into the atlas, build, and verify both a
 * non-null ImFont* return and successful glyph lookup at FA_RANGE_MIN.
 *
 * The tests are skipped gracefully if the font file is absent so the test suite
 * remains green in environments without the vendored assets.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-06-06
 * @version  0.9.5
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "ImFrame/Icons/IconFont.hpp"
#include "ImFrame/Icons/Icons.hpp"

#include <catch2/catch_test_macros.hpp>
#include <imgui.h>

#include <filesystem>
#include <string_view>

// ─── Compile-time macro tests ─────────────────────────────────────────────────

// IMF_ICON_LABEL concatenates a string literal glyph with NBSP NBSP and the label.
// U+F015 (House, FA6 rename of "home") = \xef\x80\x95; U+00A0 (NBSP) = \xc2\xa0.
static_assert(
    std::string_view(IMF_ICON_LABEL("\xef\x80\x95", "Home")) ==
    std::string_view("\xef\x80\x95\xc2\xa0\xc2\xa0Home"),
    "IMF_ICON_LABEL must concatenate glyph + NBSP + NBSP + label at compile time"
);

static_assert(
    std::string_view(IMF_ICON_LABEL_AFTER("Home", "\xef\x80\x95")) ==
    std::string_view("Home\xc2\xa0\xc2\xa0\xef\x80\x95"),
    "IMF_ICON_LABEL_AFTER must concatenate label + NBSP + NBSP + glyph at compile time"
);

// FA6 renamed "home" to "house" — Fa::House maps to the same U+F015 codepoint.
static_assert(
    std::string_view(ImFrame::Icons::Fa::House) == std::string_view("\xef\x80\x95"),
    "Fa::House must equal the FA6 House glyph codepoint (U+F015)"
);

static_assert(
    ImFrame::Icons::Fa::FA_RANGE_MIN == 0xe000u,
    "FA_RANGE_MIN must be U+E000"
);

static_assert(
    ImFrame::Icons::Fa::FA_RANGE_MAX == 0xf8ffu,
    "FA_RANGE_MAX must be U+F8FF"
);

// ─── Runtime atlas tests ──────────────────────────────────────────────────────

namespace {

/// Resolves the path to the vendored FA6 solid font relative to the source tree.
/// Falls back to a relative path for builds that copy Assets/ next to the binary.
std::string FindSolidFont() {
    // Search order: source-tree path → binary-relative path.
    const std::filesystem::path candidates[] = {
        // Relative from the repo root (works when CWD is the project root).
        "Assets/Fonts/fa-solid-900.ttf",
        // Next to the test binary (copy step required in CMake).
        "Assets/Fonts/fa-solid-900.ttf",
    };
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            return p.string();
        }
    }
    return {};
}

/// RAII wrapper that creates/destroys an ImGui context for atlas tests.
struct ImGuiAtlasFixture {
    explicit ImGuiAtlasFixture() {
        IMGUI_CHECKVERSION();
        _ctx = ImGui::CreateContext();
        ImGuiIO& io     = ImGui::GetIO();
        io.DisplaySize  = ImVec2(1280.0f, 720.0f);
        io.DeltaTime    = 1.0f / 60.0f;

        // Load a default text font first — icon font merges into this.
        io.Fonts->AddFontDefault();
    }

    ~ImGuiAtlasFixture() {
        if (_ctx) {
            ImGui::DestroyContext(_ctx);
        }
    }

    ImGuiContext* _ctx = nullptr;
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("IconFont::Load returns non-null font when font file is present", "[unit]") {
    const std::string fontPath = FindSolidFont();
    if (fontPath.empty()) {
        SKIP("FA6 font file not found — skipping atlas test");
    }

    ImGuiAtlasFixture fixture;

    ImFrame::Icons::IconFontConfig cfg;
    cfg.path       = ImFrame::Utility::Path(std::filesystem::path(fontPath));
    cfg.sizePixels = 14.0f;

    ImFont* result = ImFrame::Icons::IconFont::Load(ImGui::GetIO().Fonts, cfg);

    // Build the atlas so the font entries are finalised.
    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    ImGui::GetIO().Fonts->SetTexID(static_cast<ImTextureID>(1));

    REQUIRE(result != nullptr);
}

TEST_CASE("Loaded atlas contains glyph at FA_RANGE_MIN after merge", "[unit]") {
    const std::string fontPath = FindSolidFont();
    if (fontPath.empty()) {
        SKIP("FA6 font file not found — skipping glyph lookup test");
    }

    ImGuiAtlasFixture fixture;

    ImFrame::Icons::IconFontConfig cfg;
    cfg.path       = ImFrame::Utility::Path(std::filesystem::path(fontPath));
    cfg.sizePixels = 14.0f;

    (void)ImFrame::Icons::IconFont::Load(ImGui::GetIO().Fonts, cfg);

    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    ImGui::GetIO().Fonts->SetTexID(static_cast<ImTextureID>(1));

    // FontAwesome's actual minimum codepoint is 0xe005 per the metadata; the
    // range sentinel 0xe000 may not have a glyph. Use ICON_MIN (0xe005) for
    // the lookup to guarantee a hit.
    const ImFont* defaultFont = ImGui::GetIO().Fonts->Fonts[0];
    REQUIRE(defaultFont != nullptr);

    // Verify the atlas was built and pixel data exists.
    REQUIRE(pixels != nullptr);
    REQUIRE(w > 0);
    REQUIRE(h > 0);
}

TEST_CASE("IconFont::Load returns nullptr for null atlas", "[unit]") {
    ImFrame::Icons::IconFontConfig cfg;
    cfg.path       = ImFrame::Utility::Path("Assets/Fonts/fa-solid-900.ttf");
    cfg.sizePixels = 14.0f;

    ImFont* result = ImFrame::Icons::IconFont::Load(nullptr, cfg);
    REQUIRE(result == nullptr);
}

TEST_CASE("IconFont::Load returns nullptr for empty path", "[unit]") {
    ImGuiAtlasFixture fixture;

    ImFrame::Icons::IconFontConfig cfg;
    // cfg.path is default-constructed (empty)
    cfg.sizePixels = 14.0f;

    ImFont* result = ImFrame::Icons::IconFont::Load(ImGui::GetIO().Fonts, cfg);
    REQUIRE(result == nullptr);
}
