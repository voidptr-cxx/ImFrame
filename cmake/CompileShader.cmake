# CompileShader.cmake — Phase 32.2 NativeRenderer shader build pipeline
#
# Locates the glslangValidator executable brought in by vcpkg's glslang[tools]
# feature (requested via the "native-renderer" manifest feature, gated behind
# IMF_BUILD_NATIVE_RENDERER — see CMakeLists.txt) and exposes compile_shader(),
# a function that validates one GLSL source file compiles to SPIR-V for a given
# stage, then embeds both the raw source text and the compiled SPIR-V bytes as
# a generated C++ header. See PHASE_32_PROPOSAL.md's "Shader Cross-Compilation
# Pipeline" section and .claude/DECISIONS.md, Phase 32.2.
#
# Only included when IMF_BUILD_NATIVE_RENDERER=ON.

find_program(IMF_GLSLANG_VALIDATOR
    NAMES glslangValidator glslang
)
if(NOT IMF_GLSLANG_VALIDATOR)
    message(FATAL_ERROR
        "IMF_BUILD_NATIVE_RENDERER=ON but glslangValidator was not found. "
        "Ensure vcpkg installed the glslang 'tools' feature (see vcpkg.json's "
        "'native-renderer' manifest feature) and that CMAKE_TOOLCHAIN_FILE points at vcpkg.")
endif()

set(IMF_SHADER_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/Shaders")

# compile_shader(<source.glsl> <stage> <symbol_prefix> OUT_HEADER <out_var>)
#
# <source.glsl>     Path to a GLSL file combining STAGE_VERTEX/STAGE_FRAGMENT sections
#                    (see Shaders/Rect.glsl for the pattern).
# <stage>            "vert" or "frag" — passed to glslangValidator's -S flag and used to
#                    pick the matching -D STAGE_VERTEX / -D STAGE_FRAGMENT define.
# <symbol_prefix>    Base name for the generated C++ symbols, e.g. "Rect" -> kRectVertexSource.
# OUT_HEADER <var>   Receives the path to the generated header; add it to a target's sources
#                    (as a header-only dependency) so CMake tracks rebuilds correctly.
function(compile_shader SOURCE_FILE STAGE SYMBOL_PREFIX)
    cmake_parse_arguments(ARG "" "OUT_HEADER" "" ${ARGN})

    if(STAGE STREQUAL "vert")
        set(STAGE_DEFINE "STAGE_VERTEX")
        set(STAGE_LABEL "Vertex")
    elseif(STAGE STREQUAL "frag")
        set(STAGE_DEFINE "STAGE_FRAGMENT")
        set(STAGE_LABEL "Fragment")
    else()
        message(FATAL_ERROR "compile_shader: unknown stage '${STAGE}' (expected vert or frag)")
    endif()

    get_filename_component(SHADER_NAME "${SOURCE_FILE}" NAME_WE)
    set(SPV_FILE "${IMF_SHADER_GENERATED_DIR}/${SHADER_NAME}.${STAGE}.spv")
    set(HEADER_FILE "${IMF_SHADER_GENERATED_DIR}/${SHADER_NAME}.${STAGE}.hpp")

    add_custom_command(
        OUTPUT "${SPV_FILE}" "${HEADER_FILE}"
        COMMAND "${IMF_GLSLANG_VALIDATOR}" -S ${STAGE} -V -D${STAGE_DEFINE}
                "${SOURCE_FILE}" -o "${SPV_FILE}"
        COMMAND "${CMAKE_COMMAND}"
                -DIMF_SHADER_SOURCE=${SOURCE_FILE}
                -DIMF_SHADER_SPV=${SPV_FILE}
                -DIMF_SHADER_SYMBOL=${SYMBOL_PREFIX}${STAGE_LABEL}
                -DIMF_SHADER_HEADER=${HEADER_FILE}
                -P "${CMAKE_SOURCE_DIR}/cmake/EmbedShaderSource.cmake"
        DEPENDS "${SOURCE_FILE}" "${CMAKE_SOURCE_DIR}/cmake/EmbedShaderSource.cmake"
        COMMENT "Compiling+embedding shader ${SHADER_NAME} (${STAGE})"
        VERBATIM
    )

    set(${ARG_OUT_HEADER} "${HEADER_FILE}" PARENT_SCOPE)
endfunction()
