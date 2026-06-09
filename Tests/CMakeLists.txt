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

# ─── Utility tests ────────────────────────────────────────────────────────────

add_executable(ImFrame_Tests_Utility_File
    Utility/File_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_File
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_File PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_File PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Utility_Dir
    Utility/Directory_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Dir
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Dir PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Dir PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Utility_Watch
    Utility/FileWatcher_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Watch
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Watch PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Watch PROPERTIES LABELS unit)

# ─── Phase 4: Concurrency tests ───────────────────────────────────────────────

add_executable(ImFrame_Tests_Utility_Thread
    Utility/Thread_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Thread
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Thread PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Thread PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Utility_Pool
    Utility/ThreadPool_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Pool
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Pool PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Pool PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Utility_Worker
    Utility/BackgroundWorker_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Worker
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Worker PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Worker PROPERTIES LABELS "unit;tsan")

# ─── Phase 5: Event system tests ──────────────────────────────────────────────

add_executable(ImFrame_Tests_Utility_Delegate
    Utility/Delegate_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Delegate
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Delegate PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Delegate PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Utility_Signal
    Utility/Signal_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Signal
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Signal PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Signal PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Utility_EventBus
    Utility/EventBus_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_EventBus
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_EventBus PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_EventBus PROPERTIES LABELS "unit;tsan")

# ─── Phase 6: Supporting utility tests ───────────────────────────────────────

add_executable(ImFrame_Tests_Utility_Timer
    Utility/Timer_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Timer
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Timer PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Timer PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Utility_Logger
    Utility/Logger_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Logger
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Logger PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Logger PROPERTIES LABELS "unit;tsan")

add_executable(ImFrame_Tests_Utility_Config
    Utility/Config_test.cpp
)
target_link_libraries(ImFrame_Tests_Utility_Config
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Utility_Config PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Utility_Config PROPERTIES LABELS unit)

# ─── Phase 7: App tests ───────────────────────────────────────────────────────

add_executable(ImFrame_Tests_App_Application
    App/Application_test.cpp
)
target_link_libraries(ImFrame_Tests_App_Application
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_App_Application PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_App_Application PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_App_DockSpace
    App/DockSpace_test.cpp
)
target_link_libraries(ImFrame_Tests_App_DockSpace
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_App_DockSpace PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_App_DockSpace PROPERTIES LABELS unit)

# ─── Phase 8: Theme tests ─────────────────────────────────────────────────────

add_executable(ImFrame_Tests_Theme_ColorToken
    Theme/ColorToken_test.cpp
)
target_link_libraries(ImFrame_Tests_Theme_ColorToken
    PRIVATE ImFrame Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Theme_ColorToken PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Theme_ColorToken PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Theme_Theme
    Theme/Theme_test.cpp
)
target_link_libraries(ImFrame_Tests_Theme_Theme
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Theme_Theme PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Theme_Theme PROPERTIES LABELS unit)

# ─── Phase 9: Icon font tests ─────────────────────────────────────────────────

add_executable(ImFrame_Tests_Icons_IconFont
    Icons/IconFont_test.cpp
)
target_link_libraries(ImFrame_Tests_Icons_IconFont
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Icons_IconFont PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Icons_IconFont PROPERTIES LABELS unit)

# Copy the vendored FA6 solid font next to the test binary so FindSolidFont()
# resolves "Assets/Fonts/fa-solid-900.ttf" relative to the working directory.
add_custom_command(TARGET ImFrame_Tests_Icons_IconFont POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ImFrame_Tests_Icons_IconFont>/Assets/Fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/Assets/Fonts/fa-solid-900.ttf"
        "$<TARGET_FILE_DIR:ImFrame_Tests_Icons_IconFont>/Assets/Fonts/fa-solid-900.ttf"
    COMMENT "Copying FA6 solid font to test output directory"
)

# ─── Phase 10: Widget tests ───────────────────────────────────────────────────
# One executable per widget for fast parallel CI and clear failure attribution.

foreach(_widget
    Button
    Checkbox
    Radio
    TextInput
    Slider
    Combo
    ColorEdit
    Image
    ProgressBar
    Separator
    Spacer
    Text
)
    add_executable("ImFrame_Tests_Widgets_${_widget}"
        "Widgets/${_widget}_test.cpp"
    )
    target_link_libraries("ImFrame_Tests_Widgets_${_widget}"
        PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
    )
    target_compile_features("ImFrame_Tests_Widgets_${_widget}" PRIVATE cxx_std_23)
    catch_discover_tests("ImFrame_Tests_Widgets_${_widget}"
        PROPERTIES LABELS unit
    )
endforeach()

# ─── Phase 11: Layout container tests ────────────────────────────────────────

foreach(_layout
    Panel
    HStack
    VStack
    Grid
    ScrollArea
)
    add_executable("ImFrame_Tests_Layout_${_layout}"
        "Layout/${_layout}_test.cpp"
    )
    target_link_libraries("ImFrame_Tests_Layout_${_layout}"
        PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
    )
    target_compile_features("ImFrame_Tests_Layout_${_layout}" PRIVATE cxx_std_23)
    catch_discover_tests("ImFrame_Tests_Layout_${_layout}"
        PROPERTIES LABELS unit
    )
endforeach()

# ─── Phase 12: Animation tests ────────────────────────────────────────────────
# Pure arithmetic — no ImGui context required.

foreach(_anim
    Tween
    AnimatedValue
    Sequence
)
    add_executable("ImFrame_Tests_Anim_${_anim}"
        "Anim/${_anim}_test.cpp"
    )
    target_link_libraries("ImFrame_Tests_Anim_${_anim}"
        PRIVATE ImFrame Catch2::Catch2WithMain
    )
    target_compile_features("ImFrame_Tests_Anim_${_anim}" PRIVATE cxx_std_23)
    catch_discover_tests("ImFrame_Tests_Anim_${_anim}"
        PROPERTIES LABELS unit
    )
endforeach()

# ─── Phase 13: Overlay tests ──────────────────────────────────────────────────

add_executable(ImFrame_Tests_Overlay_Toast
    Overlay/Toast_test.cpp
)
target_link_libraries(ImFrame_Tests_Overlay_Toast
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Overlay_Toast PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Overlay_Toast PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Overlay_Modal
    Overlay/Modal_test.cpp
)
target_link_libraries(ImFrame_Tests_Overlay_Modal
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Overlay_Modal PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Overlay_Modal PROPERTIES LABELS unit)

# ─── Phase 14: Table and PropertyGrid tests ───────────────────────────────────

add_executable(ImFrame_Tests_Widgets_Table
    Widgets/Table_test.cpp
)
target_link_libraries(ImFrame_Tests_Widgets_Table
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Widgets_Table PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Widgets_Table PROPERTIES LABELS unit)

add_executable(ImFrame_Tests_Widgets_PropertyGrid
    Widgets/PropertyGrid_test.cpp
)
target_link_libraries(ImFrame_Tests_Widgets_PropertyGrid
    PRIVATE ImFrame imgui::imgui Catch2::Catch2WithMain
)
target_compile_features(ImFrame_Tests_Widgets_PropertyGrid PRIVATE cxx_std_23)
catch_discover_tests(ImFrame_Tests_Widgets_PropertyGrid PROPERTIES LABELS unit)
