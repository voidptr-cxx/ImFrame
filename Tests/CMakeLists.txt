cmake_minimum_required(VERSION 3.25)

find_package(Catch2 3 REQUIRED)
include(CTest)
include(Catch)

# ─── Backend tests ────────────────────────────────────────────────────────────
if(IMF_BACKEND_GLFW_OPENGL3)
    add_executable(ImFrame_Tests_Backends
        Backends/GLFWOpenGL3_test.cpp
    )

    target_link_libraries(ImFrame_Tests_Backends
        PRIVATE
            ImFrame
            ImFrame_GLFWOpenGL3
            Catch2::Catch2WithMain
    )

    target_compile_features(ImFrame_Tests_Backends PRIVATE cxx_std_23)

    catch_discover_tests(ImFrame_Tests_Backends
        PROPERTIES LABELS unit
    )
endif()
