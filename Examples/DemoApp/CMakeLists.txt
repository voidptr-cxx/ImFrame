cmake_minimum_required(VERSION 3.25)

add_executable(DemoApp main.cpp)

# ImFrame's own src/ is PRIVATE on the ImFrame target (not exposed to real
# consumers) — added here only so TreePanel (a local helper working around
# SetRoot()'s single-fixed-window limitation) can reach Internal::
# ImGuiCompatRenderer to replay the CommandBuffer its Paint() calls record.
# There is no public API for this yet; see .claude/DECISIONS.md, Phase 31.3.
target_include_directories(DemoApp PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(DemoApp
    PRIVATE
        ImFrame
        ImFrame_GLFWOpenGL3
        imgui::imgui        # DemoApp uses ImGui directly for menus and table cell renderers
        imframe_warnings
)

target_compile_features(DemoApp PRIVATE cxx_std_23)

set_target_properties(DemoApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_command(TARGET DemoApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:DemoApp>/Assets/Fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Assets/Fonts/fa-solid-900.ttf"
        "$<TARGET_FILE_DIR:DemoApp>/Assets/Fonts/fa-solid-900.ttf"
    COMMENT "Copying FA6 solid font to DemoApp output directory"
)
