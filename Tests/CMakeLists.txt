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
