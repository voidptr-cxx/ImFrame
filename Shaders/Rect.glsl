// Rect.glsl — filled and/or stroked rounded rectangle with independent per-corner radii.
//
// Matches Rendering::DrawRect (include/ImFrame/Rendering/CommandBuffer.hpp): Position, Size,
// per-corner CornerRadii, FillColor, StrokeColor, StrokeWidth. Compiled twice — once with
// -DSTAGE_VERTEX, once with -DSTAGE_FRAGMENT — by cmake/CompileShader.cmake's compile_shader(),
// producing two separate SPIR-V modules from this one file (see .claude/DECISIONS.md, Phase 32.2,
// for why a single file with preprocessor-guarded stages was chosen over separate .vert/.frag
// files: it matches PHASE_32_PROPOSAL.md's literal "New Files" list of exactly one Rect.glsl).
//
// The fragment stage evaluates a per-pixel signed-distance field (Inigo Quilez's rounded-box
// SDF: https://iquilezles.org/articles/distfunctions2d/) so one quad renders crisp, anti-aliased
// rounding at any pixel size without per-corner geometry tessellation.
//
// Vertex attribute layout here is this shader's own — it does NOT yet match
// Internal::Vertex (src/Rendering/Renderers/BatchBuilder.hpp), which currently only carries
// Position/Uv/Color. Reconciling the two (BatchBuilder needs to grow per-rect SDF parameters,
// or Rect draws need a dedicated non-batched vertex format) is real work for whichever sub-phase
// wires this shader into an actual backend — out of scope for the shader-authoring pipeline itself.

#version 450

#if defined(STAGE_VERTEX)

layout(location = 0) in vec2 inPosition;  // pixel-space vertex position
layout(location = 1) in vec2 inLocal;     // position relative to the rect's center, in pixels
layout(location = 2) in vec2 inHalfSize;  // rect half-width/half-height, in pixels
layout(location = 3) in vec4 inRadii;     // per-corner radii: x=TopLeft, y=TopRight, z=BottomRight, w=BottomLeft
layout(location = 4) in vec4 inFillColor;
layout(location = 5) in vec4 inStrokeColor;
layout(location = 6) in float inStrokeWidth;

layout(location = 0) out vec2 vLocal;
layout(location = 1) out vec2 vHalfSize;
layout(location = 2) out vec4 vRadii;
layout(location = 3) out vec4 vFillColor;
layout(location = 4) out vec4 vStrokeColor;
layout(location = 5) out float vStrokeWidth;

layout(binding = 0) uniform PerFrame {
    vec2 ViewportSize;
} uFrame;

void main() {
    vec2 ndc = (inPosition / uFrame.ViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    vLocal = inLocal;
    vHalfSize = inHalfSize;
    vRadii = inRadii;
    vFillColor = inFillColor;
    vStrokeColor = inStrokeColor;
    vStrokeWidth = inStrokeWidth;
}

#elif defined(STAGE_FRAGMENT)

layout(location = 0) in vec2 vLocal;
layout(location = 1) in vec2 vHalfSize;
layout(location = 2) in vec4 vRadii;
layout(location = 3) in vec4 vFillColor;
layout(location = 4) in vec4 vStrokeColor;
layout(location = 5) in float vStrokeWidth;

layout(location = 0) out vec4 outColor;

// Distance from p to a box of half-size b, with per-corner radii r = (TopRight, BottomRight,
// TopLeft, BottomLeft) — Inigo Quilez's rounded-box SDF.
float RoundedBoxSdf(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r.x;
}

void main() {
    // Reorder from CornerRadii's (TopLeft, TopRight, BottomRight, BottomLeft) to the
    // (TopRight, BottomRight, TopLeft, BottomLeft) order RoundedBoxSdf expects.
    vec4 r = vec4(vRadii.y, vRadii.z, vRadii.x, vRadii.w);
    float dist = RoundedBoxSdf(vLocal, vHalfSize, r);

    float fillAlpha = 1.0 - smoothstep(-1.0, 1.0, dist);
    vec4 color = vFillColor * fillAlpha;

    if (vStrokeWidth > 0.0) {
        float strokeDist = abs(dist) - vStrokeWidth * 0.5;
        float strokeAlpha = 1.0 - smoothstep(-1.0, 1.0, strokeDist);
        color = mix(color, vStrokeColor, strokeAlpha * vStrokeColor.a);
    }

    outColor = color;
}

#else
#error "Rect.glsl must be compiled with -DSTAGE_VERTEX or -DSTAGE_FRAGMENT"
#endif
