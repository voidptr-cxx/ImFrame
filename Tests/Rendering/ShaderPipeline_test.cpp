/**
 * @file     ShaderPipeline_test.cpp
 * @brief    Smoke tests for the Phase 32.2 generated shader headers
 *
 * @internal
 * Only built when IMF_BUILD_NATIVE_RENDERER=ON (see CMakeLists.txt / Tests/CMakeLists.txt) —
 * the generated headers this file includes don't exist otherwise. Proves
 * cmake/CompileShader.cmake actually produced real, non-empty SPIR-V and source text for
 * every shader/stage, not just that the build didn't error.
 *
 * @author   voidptr-cxx (https://github.com/voidptr-cxx)
 * @date     2026-07-27
 * @version  3.0.0
 *
 * @copyright Copyright (c) 2025 voidptr-cxx. All rights reserved.
 *            Proprietary and confidential. Unauthorised copying, distribution,
 *            or modification of this file is strictly prohibited.
 */

#include <catch2/catch_test_macros.hpp>

#include "Shaders/Rect.vert.hpp"
#include "Shaders/Rect.frag.hpp"
#include "Shaders/Image.vert.hpp"
#include "Shaders/Image.frag.hpp"
#include "Shaders/Path.vert.hpp"
#include "Shaders/Path.frag.hpp"

#include <cstring>

using namespace ImFrame::Internal::Shaders;

TEST_CASE("ShaderPipeline: every generated header has non-empty source and non-empty SPIR-V", "[unit]") {
    REQUIRE(std::strlen(kRectVertexSource) > 0);
    REQUIRE(kRectVertexSpirvByteCount > 0);
    REQUIRE(sizeof(kRectVertexSpirv) == kRectVertexSpirvByteCount);

    REQUIRE(std::strlen(kRectFragmentSource) > 0);
    REQUIRE(kRectFragmentSpirvByteCount > 0);
    REQUIRE(sizeof(kRectFragmentSpirv) == kRectFragmentSpirvByteCount);

    REQUIRE(std::strlen(kImageVertexSource) > 0);
    REQUIRE(kImageVertexSpirvByteCount > 0);
    REQUIRE(sizeof(kImageVertexSpirv) == kImageVertexSpirvByteCount);

    REQUIRE(std::strlen(kImageFragmentSource) > 0);
    REQUIRE(kImageFragmentSpirvByteCount > 0);
    REQUIRE(sizeof(kImageFragmentSpirv) == kImageFragmentSpirvByteCount);

    REQUIRE(std::strlen(kPathVertexSource) > 0);
    REQUIRE(kPathVertexSpirvByteCount > 0);
    REQUIRE(sizeof(kPathVertexSpirv) == kPathVertexSpirvByteCount);

    REQUIRE(std::strlen(kPathFragmentSource) > 0);
    REQUIRE(kPathFragmentSpirvByteCount > 0);
    REQUIRE(sizeof(kPathFragmentSpirv) == kPathFragmentSpirvByteCount);
}

TEST_CASE("ShaderPipeline: every embedded SPIR-V module starts with the SPIR-V magic number", "[unit]") {
    // SPIR-V binaries begin with the 4-byte magic number 0x07230203 (little-endian on this
    // build's target triplet) — a cheap, strong signal the embedded bytes are real compiled
    // SPIR-V and not e.g. accidentally-embedded garbage or a truncated read.
    constexpr unsigned char kMagic[4] = {0x03, 0x02, 0x23, 0x07};

    REQUIRE(std::memcmp(kRectVertexSpirv, kMagic, 4) == 0);
    REQUIRE(std::memcmp(kRectFragmentSpirv, kMagic, 4) == 0);
    REQUIRE(std::memcmp(kImageVertexSpirv, kMagic, 4) == 0);
    REQUIRE(std::memcmp(kImageFragmentSpirv, kMagic, 4) == 0);
    REQUIRE(std::memcmp(kPathVertexSpirv, kMagic, 4) == 0);
    REQUIRE(std::memcmp(kPathFragmentSpirv, kMagic, 4) == 0);
}
