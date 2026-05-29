cmake_minimum_required(VERSION 3.25)

# Tests are registered here as each phase adds them.
# Catch2 is added as a dependency from Phase 1 onwards.
#
# Pattern for adding a test executable:
#
#   add_executable(ImFrame_Tests_Backends
#       Backends/GLFWOpenGL3_test.cpp
#   )
#   target_link_libraries(ImFrame_Tests_Backends PRIVATE ImFrame Catch2::Catch2WithMain)
#   catch_discover_tests(ImFrame_Tests_Backends LABELS unit)

message(STATUS "ImFrame: Tests directory configured (no tests in Phase 0)")
