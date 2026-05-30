cmake_minimum_required(VERSION 3.25)

# ─── GLFWOpenGL3 backend static library ───────────────────────────────────────
add_library(ImFrame_GLFWOpenGL3 STATIC
    GLFWOpenGL3Backend.cpp
)

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
