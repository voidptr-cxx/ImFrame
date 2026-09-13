# CompileShaderDXC.cmake — Phase 35.7 NativeRendererDX12 HLSL build pipeline
#
# Locates dxc.exe (DirectXShaderCompiler) and exposes compile_shader_hlsl(), the DX12/HLSL analogue
# of cmake/CompileShader.cmake's own compile_shader() for GLSL/SPIR-V — same two-compiles-per-
# technique shape (one per stage), same generated-header embed idea (via EmbedShaderDXIL.cmake,
# not EmbedShaderSource.cmake — see that file's own comment on why a separate script exists rather
# than a shared, parameterized one: SPIR-V and DXIL bytes must never be confused by symbol name
# alone).
#
# dxc.exe ships with both the Windows 10/11 SDK and the Vulkan SDK (this codebase already requires
# the latter for glslangValidator) — no new external dependency, unlike glslang's own vcpkg feature.
#
# Only included when IMF_BUILD_NATIVE_RENDERER=ON and IMF_BACKEND_SDL3_DX12=ON.

find_program(IMF_DXC_COMPILER NAMES dxc dxc.exe)
if(NOT IMF_DXC_COMPILER)
    message(FATAL_ERROR
        "IMF_BUILD_NATIVE_RENDERER=ON with IMF_BACKEND_SDL3_DX12=ON but dxc.exe (DirectX Shader "
        "Compiler) was not found on PATH. It ships with the Windows 10/11 SDK and the Vulkan SDK "
        "(this project already requires the latter for glslangValidator) — ensure one of them is "
        "on PATH.")
endif()

set(IMF_SHADER_DXIL_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/Shaders")

# compile_shader_hlsl(<source.hlsl> <profile> <entry_point> <symbol_prefix> OUT_HEADER <out_var>)
#
# <source.hlsl>     Path to an HLSL file with (at least) the named <entry_point> function.
# <profile>          "vs_6_0" or "ps_6_0" — passed to dxc's -T flag and used to pick the matching
#                    generated-symbol stage label.
# <entry_point>      The HLSL function name to compile (e.g. "VSMain"/"PSMain") — dxc's -E flag.
#                    Unlike compile_shader()'s own -D STAGE_VERTEX/STAGE_FRAGMENT macro selection,
#                    HLSL naturally supports multiple entry points in one file with no preprocessor
#                    guard needed, so SDFRect.hlsl keeps both stages in one file directly.
# <symbol_prefix>    Base name for the generated C++ symbols, e.g. "SDFRect" -> kSDFRectVertexDxil.
# OUT_HEADER <var>   Receives the path to the generated header; add it to a target's sources (as a
#                    header-only dependency) so CMake tracks rebuilds correctly.
function(compile_shader_hlsl SOURCE_FILE PROFILE ENTRY_POINT SYMBOL_PREFIX)
    cmake_parse_arguments(ARG "" "OUT_HEADER" "" ${ARGN})

    if(PROFILE STREQUAL "vs_6_0")
        set(STAGE_LABEL "Vertex")
    elseif(PROFILE STREQUAL "ps_6_0")
        set(STAGE_LABEL "Pixel")
    else()
        message(FATAL_ERROR "compile_shader_hlsl: unknown profile '${PROFILE}' (expected vs_6_0 or ps_6_0)")
    endif()

    get_filename_component(SHADER_NAME "${SOURCE_FILE}" NAME_WE)
    set(DXIL_FILE "${IMF_SHADER_DXIL_GENERATED_DIR}/${SHADER_NAME}.${STAGE_LABEL}.dxil")
    set(HEADER_FILE "${IMF_SHADER_DXIL_GENERATED_DIR}/${SHADER_NAME}.${STAGE_LABEL}.hpp")

    add_custom_command(
        OUTPUT "${DXIL_FILE}" "${HEADER_FILE}"
        COMMAND "${IMF_DXC_COMPILER}" -T ${PROFILE} -E ${ENTRY_POINT} -Fo "${DXIL_FILE}" "${SOURCE_FILE}"
        COMMAND "${CMAKE_COMMAND}"
                -DIMF_SHADER_SOURCE=${SOURCE_FILE}
                -DIMF_SHADER_DXIL=${DXIL_FILE}
                -DIMF_SHADER_SYMBOL=${SYMBOL_PREFIX}${STAGE_LABEL}
                -DIMF_SHADER_HEADER=${HEADER_FILE}
                -P "${CMAKE_SOURCE_DIR}/cmake/EmbedShaderDXIL.cmake"
        DEPENDS "${SOURCE_FILE}" "${CMAKE_SOURCE_DIR}/cmake/EmbedShaderDXIL.cmake"
        COMMENT "Compiling+embedding HLSL shader ${SHADER_NAME} (${STAGE_LABEL})"
        VERBATIM
    )

    set(${ARG_OUT_HEADER} "${HEADER_FILE}" PARENT_SCOPE)
endfunction()
