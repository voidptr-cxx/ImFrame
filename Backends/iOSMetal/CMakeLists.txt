cmake_minimum_required(VERSION 3.25)

# Scaffold only — builds the target structure on an iOS toolchain, but the
# single .mm source intentionally fails to compile (#error) until a future
# phase implements CADisplayLink-driven rendering and UIKit windowing/input.
# Returns immediately on every other platform (including macOS) so this
# directory is a structural no-op everywhere except an actual iOS build.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    return()
endif()

enable_language(OBJCXX)

add_library(ImFrame_iOSMetal STATIC
    iOSMetalBackend.mm
)

set_source_files_properties(iOSMetalBackend.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")

target_include_directories(ImFrame_iOSMetal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ImFrame_iOSMetal
    PUBLIC  ImFrame
    PRIVATE imframe_warnings
)

target_compile_features(ImFrame_iOSMetal PRIVATE cxx_std_23)
