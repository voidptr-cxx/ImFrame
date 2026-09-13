// SDFRect.hlsl — DX12 port of Shaders/SDFRect.glsl (see that file's own header comment for the
// rounded-box SDF math and fwidth-derived AA band derivation — both unchanged here, ported
// directly from GLSL to HLSL syntax).
//
// Y IS negated in VSMain, matching NativeRendererGL3's own SDFRect vertex shader — NOT
// SDFRect.glsl's own Vulkan copy, which deliberately does not negate. D3D's NDC is Y-up
// (a clip-space position with +Y maps to the TOP of the viewport, the same convention GL uses),
// unlike Vulkan's Y-down NDC — see SDFRect.glsl's own comment for the full Vulkan-vs-GL
// derivation; the same reasoning applies to D3D as to GL here (both Y-up NDC), which is why this
// file's own vertex shader matches NativeRendererGL3's choice, not NativeRendererVulkan's. See
// .claude/DECISIONS.md, Phase 35.7, for this file's own derivation.
//
// Compiled twice by cmake/CompileShaderDXC.cmake's compile_shader_hlsl() — once as `vs_6_0`
// targeting VSMain, once as `ps_6_0` targeting PSMain — mirroring compile_shader()'s own
// two-compiles-per-technique convention for the GLSL/SPIR-V pipeline.

cbuffer PerFrame : register(b0) {
    float2 ViewportSize;
};

struct VSInput {
    float2 Position    : POSITION;   // pixel-space vertex position
    float2 Local       : LOCAL;      // position relative to the rect's center, in pixels
    float2 HalfSize    : HALFSIZE;   // rect half-width/half-height, in pixels
    float4 Radii       : RADII;      // per-corner radii: x=TopLeft, y=TopRight, z=BottomRight, w=BottomLeft
    float4 FillColor   : FILLCOLOR;
    float4 StrokeColor : STROKECOLOR;
    float  StrokeWidth : STROKEWIDTH;
};

struct PSInput {
    float4 ClipPosition : SV_Position;
    float2 Local        : LOCAL;
    float2 HalfSize     : HALFSIZE;
    float4 Radii        : RADII;
    float4 FillColor    : FILLCOLOR;
    float4 StrokeColor  : STROKECOLOR;
    float  StrokeWidth  : STROKEWIDTH;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    float2 ndc = (input.Position / ViewportSize) * 2.0 - 1.0;
    output.ClipPosition = float4(ndc.x, -ndc.y, 0.0, 1.0);
    output.Local        = input.Local;
    output.HalfSize      = input.HalfSize;
    output.Radii        = input.Radii;
    output.FillColor    = input.FillColor;
    output.StrokeColor  = input.StrokeColor;
    output.StrokeWidth  = input.StrokeWidth;
    return output;
}

// Distance from p to a box of half-size b, with per-corner radii r = (TopRight, BottomRight,
// TopLeft, BottomLeft) — Inigo Quilez's rounded-box SDF. Identical to SDFRect.glsl's own
// RoundedBoxSdf().
float RoundedBoxSdf(float2 p, float2 b, float4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x  = (p.y > 0.0) ? r.x : r.y;
    float2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, float2(0.0, 0.0))) - r.x;
}

float4 PSMain(PSInput input) : SV_Target {
    // Reorder from CornerRadii's (TopLeft, TopRight, BottomRight, BottomLeft) to the
    // (TopRight, BottomRight, TopLeft, BottomLeft) order RoundedBoxSdf expects.
    float4 r = float4(input.Radii.y, input.Radii.z, input.Radii.x, input.Radii.w);
    float dist = RoundedBoxSdf(input.Local, input.HalfSize, r);

    float aa = max(fwidth(dist) * 0.5, 1e-4);
    float fillAlpha = 1.0 - smoothstep(-aa, aa, dist);
    float4 color = input.FillColor * fillAlpha;

    if (input.StrokeWidth > 0.0) {
        float strokeDist = abs(dist) - input.StrokeWidth * 0.5;
        float strokeAa = max(fwidth(strokeDist) * 0.5, 1e-4);
        float strokeAlpha = 1.0 - smoothstep(-strokeAa, strokeAa, strokeDist);
        color = lerp(color, input.StrokeColor, strokeAlpha * input.StrokeColor.a);
    }

    return color;
}
