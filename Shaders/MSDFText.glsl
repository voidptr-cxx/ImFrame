// MSDFText.glsl — median-of-three MSDF glyph reconstruction with fwidth()-based anti-aliasing.
//
// Matches Phase 33's GlyphAtlas (src/Rendering/Text/GlyphAtlas.hpp): samples the RGB channels of
// an MTSDF bitmap uploaded to TextureAtlas, recovers the signed distance via the standard
// median-of-three operator (Chlumsky, msdfgen's own reference shader), and reconstructs a sharp,
// resolution-independent edge using the screen-space derivative of the UV coordinates (fwidth) —
// no supersampling, no per-size glyph regeneration. See Rect.glsl's file comment for the
// STAGE_VERTEX/STAGE_FRAGMENT single-file compile technique (duplicated here, not shared via
// #include, for the same "no cross-shader #include exists in this codebase yet" reason).
//
// Deviates from PHASE_33_PROPOSAL.md's literal "shader accepts a color uniform for the glyph
// color" wording: color is a per-vertex attribute (inTextColor), matching Rect.glsl/Image.glsl's
// own inFillColor/inTintColor convention, so a batch can mix differently-colored glyphs in one
// draw call instead of splitting a draw per color group. See .claude/DECISIONS.md, Phase 33.5.
//
// This sub-phase authors the shader only — no BatchKind::Text/TextVertex/NativeRendererGL3 port
// exists yet, matching Path.glsl's own Phase 32.2 precedent (a shader with no live batch producer,
// validated purely by ShaderPipeline_test.cpp compiling it). Wiring this into a real batch kind is
// the proposal's separate "DrawText Command Processing" work, for a later sub-phase.

#version 450

#if defined(STAGE_VERTEX)

layout(location = 0) in vec2 inPosition; // pixel-space vertex position
layout(location = 1) in vec2 inUv;       // MSDF atlas texture coordinate
layout(location = 2) in vec4 inTextColor;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vTextColor;

layout(binding = 0) uniform PerFrame {
    vec2 ViewportSize;
} uFrame;

void main() {
    vec2 ndc = (inPosition / uFrame.ViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    vUv = inUv;
    vTextColor = inTextColor;
}

#elif defined(STAGE_FRAGMENT)

layout(location = 0) in vec2 vUv;
layout(location = 1) in vec4 vTextColor;

layout(binding = 1) uniform sampler2D uAtlas;

// Matches Internal::GlyphAtlas::kPxRange (src/Rendering/Text/GlyphAtlas.hpp) — the distance-field
// range, in *source* MSDF-bitmap pixels, msdfgen::generateMTSDF() was generated with. Not a shader
// constant because the atlas texture is shared across every font/glyph GlyphAtlas ever uploads,
// all generated with the one fixed kPxRange, so a single per-draw uniform (not a per-vertex
// attribute) is enough.
layout(binding = 2) uniform MsdfParams {
    float PxRange;
} uMsdf;

layout(location = 0) out vec4 outColor;

float Median(vec3 msd) {
    return max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
}

// Converts uMsdf.PxRange (in atlas-texel units) to screen pixels at this fragment's actual
// on-screen size, via the screen-space derivative of the UV coordinate — the same technique the
// proposal specifies ("width derived from the screen-space derivative of the UV coordinates").
// This is what lets one fixed-density MSDF bitmap reconstruct sharp edges at any final draw size.
float ScreenPxRange() {
    vec2 unitRange = vec2(uMsdf.PxRange) / vec2(textureSize(uAtlas, 0));
    vec2 screenTexSize = vec2(1.0) / fwidth(vUv);
    return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

void main() {
    vec3 msd = texture(uAtlas, vUv).rgb;
    float signedDistance = Median(msd) - 0.5;
    float screenPxDistance = ScreenPxRange() * signedDistance;
    float opacity = smoothstep(-0.5, 0.5, screenPxDistance);

    outColor = vec4(vTextColor.rgb, vTextColor.a * opacity);
}

#else
#error "MSDFText.glsl must be compiled with -DSTAGE_VERTEX or -DSTAGE_FRAGMENT"
#endif
