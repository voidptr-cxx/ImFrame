cmake_minimum_required(VERSION 3.25)

# ─── GLFWOpenGL3 backend static library ───────────────────────────────────────
add_library(ImFrame_GLFWOpenGL3 STATIC
    GLFWOpenGL3Backend.cpp
    ViewportGL.cpp
)

# ── Phase 32.4: NativeRendererGL3 (opt-in, see .claude/DECISIONS.md for why it lives here
# rather than at PHASE_32_PROPOSAL.md's literal src/Rendering/Renderers/Backends/ path) ──
if(IMF_BUILD_NATIVE_RENDERER)
    target_sources(ImFrame_GLFWOpenGL3 PRIVATE NativeRendererGL3.cpp)
    # NativeRendererGL3.cpp reaches into src/Rendering/Renderers/ (BatchBuilder.hpp, IRenderer.hpp) —
    # an internal ImFrame header directory, not part of ImFrame's own PUBLIC include interface.
    target_include_directories(ImFrame_GLFWOpenGL3 PRIVATE ${CMAKE_SOURCE_DIR}/src)
endif()

# ── Phase 34.3: BlurPassGL3, NativeRendererGL3's fragment-shader-ping-pong Gaussian blur.
# Gated on IMF_BUILD_NATIVE_RENDERER alone (same as NativeRendererGL3.cpp above) -- it has no
# dependency on the text pipeline (IMF_BUILD_TEXT_MSDF), unlike TextRendererGL3.cpp below. ──
if(IMF_BUILD_NATIVE_RENDERER)
    target_sources(ImFrame_GLFWOpenGL3 PRIVATE BlurPassGL3.cpp)
endif()

# ── Phase 33.7: TextRendererGL3, NativeRendererGL3's real GL3 MSDF text delegate. Requires BOTH
# IMF_BUILD_NATIVE_RENDERER (this file's own gate, above) AND IMF_BUILD_TEXT_MSDF (FontRegistry/
# TextShaper/GlyphAtlas, Phase 33.1+) — deliberately not auto-enabled by one another (see
# .claude/DECISIONS.md, Phase 33.7): IMF_BUILD_NATIVE_RENDERER alone must keep building exactly as
# it did through Phase 33.6, with no silent new harfbuzz/msdfgen toolchain requirement (~26 minutes
# from source, per Phase 33.1). NativeRendererGL3.cpp/.hpp themselves have no part in this block —
# they depend only on the abstract ITextRenderer/ITextLayoutProvider seam, always available. ──
if(IMF_BUILD_NATIVE_RENDERER AND IMF_BUILD_TEXT_MSDF)
    target_sources(ImFrame_GLFWOpenGL3 PRIVATE TextRendererGL3.cpp)
    target_include_directories(ImFrame_GLFWOpenGL3 PRIVATE ${CMAKE_SOURCE_DIR}/src)
    # Same reasoning as GlyphAtlas's own test targets (Phase 33.4, DECISIONS.md): ImFrame's PRIVATE
    # link to these doesn't propagate through the static ImFrame.lib to this separate static lib.
    target_link_libraries(ImFrame_GLFWOpenGL3
        PRIVATE Freetype::Freetype
                harfbuzz::harfbuzz
                msdfgen::msdfgen-ext
                msdfgen::msdfgen-core
    )
endif()

# Expose own source directory so consumers can:
#   #include "GLFWOpenGL3Backend.hpp"
target_include_directories(ImFrame_GLFWOpenGL3
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ImFrame_GLFWOpenGL3
    PUBLIC  ImFrame                 # pulls in include/ImFrame/ and cxx_std_23
    PRIVATE imgui::imgui
            glfw
            glad::glad
            imframe_warnings
)

target_compile_features(ImFrame_GLFWOpenGL3 PRIVATE cxx_std_23)
