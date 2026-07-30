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
