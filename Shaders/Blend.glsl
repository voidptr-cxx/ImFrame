// Blend.glsl — real per-pixel blend-mode compositing for BatchKind::Layer's PushBlendLayer/PopLayer.
//
// Ports NativeRendererGL3's own kBlendVertexSource/kBlendFragmentSource (Phase 34.5) to SPIR-V.
// One pipeline handles every Rendering::BlendMode value -- Mode is a push constant, matching
// GaussianBlur.glsl's own single-pipeline-multiple-modes convention (Phase 35.3) rather than a
// uniform (there is no per-draw descriptor state this pass needs at all otherwise). Mode indices
// match Rendering::BlendMode's declaration order exactly (CommandBuffer.hpp's own comment on that
// enum says not to reorder it).
//
// The vertex stage needs no vertex buffer: this is always exactly the same fixed, full-viewport
// quad, so its 6 vertices (matching NativeRendererGL3's own kBlendFullscreenQuad layout exactly)
// are generated directly from gl_VertexIndex. UV (0,0) maps to NDC (-1,-1) (screen top-left, given
// this codebase's Vulkan backend never flips Y post-35.2) -- both source textures this shader
// samples (a popped layer's own render, and a copied backdrop) were themselves rendered by this
// same renderer with that same non-flipped convention, so a plain, unflipped mapping is correct
// here too, matching RenderShadowBatch()'s own composite quad (Phase 35.4).
//
// Both input textures are this renderer's usual premultiplied-by-coverage convention; each blend
// formula operates on unpremultiplied ("straight") colors per the standard CSS/PDF compositing
// model, so the shader unpremultiplies both inputs before blending and recomposites the
// (correctly premultiplied) result with the standard "simple/non-isolated" Porter-Duff formula
// (W3C Compositing and Blending Level 1) -- identical math to NativeRendererGL3's own fragment
// shader, kept in sync deliberately.

#version 450

#if defined(STAGE_VERTEX)

layout(location = 0) out vec2 vUv;

void main() {
    vec2 positions[6] = vec2[](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
    );
    vec2 uvs[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
        vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
    );
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    vUv = uvs[gl_VertexIndex];
}

#elif defined(STAGE_FRAGMENT)

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uSourceTexture;
layout(binding = 1) uniform sampler2D uBackdropTexture;

layout(push_constant) uniform PushConstants {
    int Mode;
} pc;

vec3 Multiply(vec3 cb, vec3 cs) { return cb * cs; }
vec3 Screen(vec3 cb, vec3 cs) { return cb + cs - cb * cs; }

vec3 Overlay(vec3 cb, vec3 cs) {
    vec3 lo = 2.0 * cb * cs;
    vec3 hi = vec3(1.0) - 2.0 * (vec3(1.0) - cb) * (vec3(1.0) - cs);
    return mix(lo, hi, step(0.5, cb)); // branches on the BACKDROP's own brightness
}

vec3 HardLight(vec3 cb, vec3 cs) {
    vec3 lo = 2.0 * cb * cs;
    vec3 hi = vec3(1.0) - 2.0 * (vec3(1.0) - cb) * (vec3(1.0) - cs);
    return mix(lo, hi, step(0.5, cs)); // branches on the SOURCE's own brightness
}

vec3 ColorDodge(vec3 cb, vec3 cs) {
    return min(vec3(1.0), cb / max(vec3(1.0) - cs, vec3(1e-4)));
}

vec3 ColorBurn(vec3 cb, vec3 cs) {
    return vec3(1.0) - min(vec3(1.0), (vec3(1.0) - cb) / max(cs, vec3(1e-4)));
}

vec3 SoftLightD(vec3 x) {
    vec3 poly = ((16.0 * x - 12.0) * x + 4.0) * x;
    vec3 sq = sqrt(x);
    return mix(poly, sq, step(0.25, x));
}

vec3 SoftLight(vec3 cb, vec3 cs) {
    vec3 dark = cb - (vec3(1.0) - 2.0 * cs) * cb * (vec3(1.0) - cb);
    vec3 light = cb + (2.0 * cs - vec3(1.0)) * (SoftLightD(cb) - cb);
    return mix(dark, light, step(0.5, cs));
}

vec3 Blend(vec3 cb, vec3 cs, int mode) {
    if (mode == 1) return Multiply(cb, cs);
    if (mode == 2) return Screen(cb, cs);
    if (mode == 3) return Overlay(cb, cs);
    if (mode == 4) return min(cb, cs);              // Darken
    if (mode == 5) return max(cb, cs);               // Lighten
    if (mode == 6) return ColorDodge(cb, cs);
    if (mode == 7) return ColorBurn(cb, cs);
    if (mode == 8) return HardLight(cb, cs);
    if (mode == 9) return SoftLight(cb, cs);
    if (mode == 10) return abs(cb - cs);             // Difference
    if (mode == 11) return cb + cs - 2.0 * cb * cs;  // Exclusion
    return cs;                                       // Normal (mode == 0)
}

void main() {
    vec4 src = texture(uSourceTexture, vUv);
    vec4 backdrop = texture(uBackdropTexture, vUv);

    float srcAlpha = src.a;
    float backdropAlpha = backdrop.a;
    vec3 cs = srcAlpha > 0.0 ? src.rgb / srcAlpha : vec3(0.0);
    vec3 cb = backdropAlpha > 0.0 ? backdrop.rgb / backdropAlpha : vec3(0.0);

    vec3 blended = Blend(cb, cs, pc.Mode);

    vec3 resultRgb = (1.0 - backdropAlpha) * srcAlpha * cs
                    + backdropAlpha * srcAlpha * blended
                    + (1.0 - srcAlpha) * backdropAlpha * cb;
    float resultAlpha = srcAlpha + backdropAlpha - srcAlpha * backdropAlpha;

    outColor = vec4(resultRgb, resultAlpha);
}

#else
#error "Blend.glsl must be compiled with -DSTAGE_VERTEX or -DSTAGE_FRAGMENT"
#endif
