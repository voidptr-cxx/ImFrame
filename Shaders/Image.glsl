// Image.glsl — textured rectangle with independent per-corner rounding and a tint colour.
//
// Matches Rendering::DrawImage: Position, Size, Texture, UvMin/UvMax, TintColor, Radii.
// See SDFRect.glsl's file comment for the STAGE_VERTEX/STAGE_FRAGMENT single-file compile
// technique and the RoundedBoxSdf rounding approach — duplicated here rather than shared via
// #include so each shader file compiles standalone (no cross-shader #include exists in this
// codebase yet); keep the two copies in sync by hand if RoundedBoxSdf's formula ever changes.

#version 450

#if defined(STAGE_VERTEX)

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inLocal;
layout(location = 2) in vec2 inHalfSize;
layout(location = 3) in vec4 inRadii;
layout(location = 4) in vec2 inUv;
layout(location = 5) in vec4 inTintColor;

layout(location = 0) out vec2 vLocal;
layout(location = 1) out vec2 vHalfSize;
layout(location = 2) out vec4 vRadii;
layout(location = 3) out vec2 vUv;
layout(location = 4) out vec4 vTintColor;

layout(binding = 0) uniform PerFrame {
    vec2 ViewportSize;
} uFrame;

void main() {
    vec2 ndc = (inPosition / uFrame.ViewportSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);

    vLocal = inLocal;
    vHalfSize = inHalfSize;
    vRadii = inRadii;
    vUv = inUv;
    vTintColor = inTintColor;
}

#elif defined(STAGE_FRAGMENT)

layout(location = 0) in vec2 vLocal;
layout(location = 1) in vec2 vHalfSize;
layout(location = 2) in vec4 vRadii;
layout(location = 3) in vec2 vUv;
layout(location = 4) in vec4 vTintColor;

layout(binding = 1) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

float RoundedBoxSdf(vec2 p, vec2 b, vec4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x = (p.y > 0.0) ? r.x : r.y;
    vec2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, vec2(0.0))) - r.x;
}

void main() {
    vec4 r = vec4(vRadii.y, vRadii.z, vRadii.x, vRadii.w);
    float mask = 1.0 - smoothstep(-1.0, 1.0, RoundedBoxSdf(vLocal, vHalfSize, r));
    outColor = texture(uTexture, vUv) * vTintColor * mask;
}

#else
#error "Image.glsl must be compiled with -DSTAGE_VERTEX or -DSTAGE_FRAGMENT"
#endif
