// Path.glsl — flat-coloured triangle shader consuming CPU-tesselated Rendering::DrawPath geometry.
//
// Per PHASE_32_PROPOSAL.md: "Filled paths use a CPU tesselator in Phase 32 (GPU tessellation
// in Phase 34)." Stroked paths are CPU-expanded into a triangle strip the same way (each
// segment becomes a quad) — see .claude/DECISIONS.md, Phase 31.2, on DrawPath's convex-only
// fill approximation, which this shader inherits unchanged: it has no opinion on how the
// triangles it receives were generated, it only rasterises them with per-vertex colour.

#version 450

#if defined(STAGE_VERTEX)

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 vColor;

layout(binding = 0) uniform PerFrame {
    vec2 ViewportSize;
} uFrame;

void main() {
    vec2 ndc = (inPosition / uFrame.ViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
    vColor = inColor;
}

#elif defined(STAGE_FRAGMENT)

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vColor;
}

#else
#error "Path.glsl must be compiled with -DSTAGE_VERTEX or -DSTAGE_FRAGMENT"
#endif
