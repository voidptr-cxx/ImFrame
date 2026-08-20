/**
 * @file     FontRegistry_test.cpp
 * @brief    Unit tests for `Internal::FontRegistry`/`Internal::FontFace` (Phase 33.2)
 *
 * @internal
 * Loads the real FA6 solid-icon TTF already vendored for `IconFont_test.cpp`
 * (`Assets/Fonts/fa-solid-900.ttf`, copied next to this test binary the same
 * way) — it is a real, valid font file, and no meaningful shaping/kerning
 * assertions are needed at this sub-phase (that is `TextShaper`'s job,
 * Phase 33.3), so an icon font is exactly as good a subject as a text font.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-08-20
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include "Rendering/Text/FontRegistry.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace ImFrame;
using namespace ImFrame::Internal;

namespace {
const Utility::Path kRealFont("Assets/Fonts/fa-solid-900.ttf");
} // namespace

TEST_CASE("FontRegistry::Load succeeds on a real font file and returns a valid FontId", "[unit]") {
    FontRegistry registry;

    const Result<Rendering::FontId> id = registry.Load(kRealFont, 16.0f);

    REQUIRE(id.has_value());
    REQUIRE(id->IsValid());
}

TEST_CASE("FontRegistry::Load fails with FileNotFound for a nonexistent path", "[unit]") {
    FontRegistry registry;

    const Result<Rendering::FontId> id = registry.Load(Utility::Path("Assets/Fonts/does-not-exist.ttf"), 16.0f);

    REQUIRE_FALSE(id.has_value());
    REQUIRE(id.error() == Error::FileNotFound);
}

TEST_CASE("FontRegistry::Get returns nullptr for a default-constructed FontId", "[unit]") {
    FontRegistry registry;

    REQUIRE(registry.Get(Rendering::FontId{}) == nullptr);
}

TEST_CASE("FontRegistry::Get returns nullptr for an id this registry never issued", "[unit]") {
    FontRegistry registry;
    (void)registry.Load(kRealFont, 16.0f);

    REQUIRE(registry.Get(Rendering::FontId(9999)) == nullptr);
}

TEST_CASE("FontRegistry::Get returns a usable FontFace for a successfully loaded FontId", "[unit]") {
    FontRegistry registry;

    const Result<Rendering::FontId> id = registry.Load(kRealFont, 20.0f);
    REQUIRE(id.has_value());

    const FontFace* face = registry.Get(*id);
    REQUIRE(face != nullptr);
    REQUIRE(face->Face() != nullptr);
    REQUIRE(face->HarfBuzzFont() != nullptr);
    REQUIRE(face->SizePixels() == 20.0f);
}

TEST_CASE("FontRegistry::Load issues distinct FontIds across multiple calls", "[unit]") {
    FontRegistry registry;

    const Result<Rendering::FontId> first  = registry.Load(kRealFont, 16.0f);
    const Result<Rendering::FontId> second = registry.Load(kRealFont, 24.0f);

    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE_FALSE(*first == *second);

    // Both remain independently resolvable — loading again doesn't invalidate the first.
    REQUIRE(registry.Get(*first)->SizePixels() == 16.0f);
    REQUIRE(registry.Get(*second)->SizePixels() == 24.0f);
}
