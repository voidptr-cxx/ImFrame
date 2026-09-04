// SDFRect.glsl — filled and/or stroked rounded rectangle with independent per-corner radii.
//
// Phase 34 replacement for Phase 32's Rect.glsl. The rounded-box SDF math itself is unchanged —
// Phase 32.2 already evaluated it analytically per-fragment (Inigo Quilez's rounded-box SDF:
// https://iquilezles.org/articles/distfunctions2d/), so there was no literal "tessellated
// rectangle" for this file to replace, despite PHASE_34_PROPOSAL.md's framing; see
// .claude/DECISIONS.md, Phase 34.1, for that discrepancy. The real, meaningful change here is the
// anti-aliasing band: Phase 32.2 smoothed over a *hardcoded* ±1.0 SDF-unit band
// (`smoothstep(-1.0, 1.0, dist)`), correct only when one SDF unit maps to exactly one screen
// pixel (the common case, but not under any zoom/DPI/transform that changes that mapping). This
// version derives the band from the fragment's own screen-space derivative (`fwidth(dist)`), so
// the edge stays exactly one pixel wide regardless of scale — the same technique
// `Shaders/MSDFText.glsl`'s `ScreenPxRange()` already uses for the identical reason.
//
// Matches Rendering::DrawRect: Position, Size, per-corner CornerRadii, FillColor, StrokeColor,
// StrokeWidth. Compiled twice — once with -DSTAGE_VERTEX, once with -DSTAGE_FRAGMENT — by
// cmake/CompileShader.cmake's compile_shader(). See Rect.glsl's original Phase 32.2 file comment
// (git history) for the single-file STAGE_VERTEX/STAGE_FRAGMENT compile technique's own rationale.
//
// Fill and stroke are handled in a single pass, as they were in Phase 32.2 — no change there.

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
// TopLeft, BottomLeft) — Inigo Quilez's rounded-box SDF. Unchanged from Phase 32.2.
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

    // fwidth(dist) is dist's total rate of change across this fragment and its neighbours, in SDF
    // units per pixel -- using it (not a hardcoded constant) as the smoothstep half-width keeps
    // the anti-aliased edge exactly one pixel wide under any scale/zoom/DPI.
    float aa = max(fwidth(dist) * 0.5, 1e-4); // clamped away from 0 to avoid a divide-by-zero-like
                                               // degenerate smoothstep on a fragment with zero derivative
    float fillAlpha = 1.0 - smoothstep(-aa, aa, dist);
    vec4 color = vFillColor * fillAlpha;

    if (vStrokeWidth > 0.0) {
        float strokeDist = abs(dist) - vStrokeWidth * 0.5;
        float strokeAa = max(fwidth(strokeDist) * 0.5, 1e-4);
        float strokeAlpha = 1.0 - smoothstep(-strokeAa, strokeAa, strokeDist);
        color = mix(color, vStrokeColor, strokeAlpha * vStrokeColor.a);
    }

    outColor = color;
}

#else
#error "SDFRect.glsl must be compiled with -DSTAGE_VERTEX or -DSTAGE_FRAGMENT"
#endif
