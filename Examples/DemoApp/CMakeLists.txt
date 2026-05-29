cmake_minimum_required(VERSION 3.25)

add_executable(DemoApp main.cpp)

target_link_libraries(DemoApp PRIVATE ImFrame imframe_warnings)
target_compile_features(DemoApp PRIVATE cxx_std_23)

set_target_properties(DemoApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
