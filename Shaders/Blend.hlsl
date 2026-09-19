// Blend.hlsl — DX12 port of Shaders/Blend.glsl (see that file's own header comment for the full
// unpremultiply-blend-recomposite derivation, ported directly here unchanged, and for why Mode is
// a root constant rather than a real constant buffer).
//
// The vertex stage needs no vertex buffer at all: this is always exactly the same fixed,
// full-viewport quad, generated directly from SV_VertexID (the HLSL analogue of Blend.glsl's own
// gl_VertexIndex-based quad, and of SDFRect.hlsl's/Image.hlsl's own real vertex buffers -- this
// shader alone needs none). Unlike Blend.glsl, the UV table here is NOT a direct copy of its GLSL
// counterpart's -- Vulkan's NDC is Y-down (so position (-1,-1) genuinely is screen-top-left there,
// and Blend.glsl's own unflipped uv(0,0) at that vertex is correct), while D3D's NDC is Y-up (so
// position (-1,-1) is screen-BOTTOM-left here instead) -- the same Y-direction divergence that
// already forced SDFRect.hlsl's/Image.hlsl's own vertex shaders to negate Y for their real vertex
// buffers. This shader has no per-frame transform to negate, so the fix is applied directly to the
// UV table instead: every entry's V component is flipped (1-v) relative to Blend.glsl's own table,
// so that screen-top still samples texture row 0 (v=0) -- matching this backend's own top-down
// texture-row convention (see NativeRendererDX12.cpp's own BuildImageQuadVertices() comment for the
// full reasoning); both source textures this shader samples (a popped layer's own render, and a
// copied backdrop) were themselves rendered by this same renderer with that identical convention.
//
// Compiled twice by cmake/CompileShaderDXC.cmake's compile_shader_hlsl() -- once as vs_6_0
// targeting VSMain, once as ps_6_0 targeting PSMain -- matching SDFRect.hlsl's/Image.hlsl's own
// convention.

Texture2D    uSourceTexture   : register(t0);
Texture2D    uBackdropTexture : register(t1);
SamplerState uSampler         : register(s0);

cbuffer PushConstants : register(b0) {
    int Mode;
};

struct VSOutput {
    float4 ClipPosition : SV_Position;
    float2 Uv           : UV;
};

VSOutput VSMain(uint vertexId : SV_VertexID) {
    const float2 positions[6] = {
        float2(-1.0, -1.0), float2(1.0, -1.0), float2(1.0, 1.0),
        float2(-1.0, -1.0), float2(1.0, 1.0), float2(-1.0, 1.0),
    };
    const float2 uvs[6] = {
        float2(0.0, 1.0), float2(1.0, 1.0), float2(1.0, 0.0),
        float2(0.0, 1.0), float2(1.0, 0.0), float2(0.0, 0.0),
    };

    VSOutput output;
    output.ClipPosition = float4(positions[vertexId], 0.0, 1.0);
    output.Uv           = uvs[vertexId];
    return output;
}

float3 Multiply(float3 cb, float3 cs) { return cb * cs; }
float3 Screen(float3 cb, float3 cs) { return cb + cs - cb * cs; }

float3 Overlay(float3 cb, float3 cs) {
    float3 lo = 2.0 * cb * cs;
    float3 hi = 1.0 - 2.0 * (1.0 - cb) * (1.0 - cs);
    return lerp(lo, hi, step(0.5, cb)); // branches on the BACKDROP's own brightness
}

float3 HardLight(float3 cb, float3 cs) {
    float3 lo = 2.0 * cb * cs;
    float3 hi = 1.0 - 2.0 * (1.0 - cb) * (1.0 - cs);
    return lerp(lo, hi, step(0.5, cs)); // branches on the SOURCE's own brightness
}

float3 ColorDodge(float3 cb, float3 cs) {
    return min(1.0, cb / max(1.0 - cs, 1e-4));
}

float3 ColorBurn(float3 cb, float3 cs) {
    return 1.0 - min(1.0, (1.0 - cb) / max(cs, 1e-4));
}

float3 SoftLightD(float3 x) {
    float3 poly = ((16.0 * x - 12.0) * x + 4.0) * x;
    float3 sq   = sqrt(x);
    return lerp(poly, sq, step(0.25, x));
}

float3 SoftLight(float3 cb, float3 cs) {
    float3 dark  = cb - (1.0 - 2.0 * cs) * cb * (1.0 - cb);
    float3 light = cb + (2.0 * cs - 1.0) * (SoftLightD(cb) - cb);
    return lerp(dark, light, step(0.5, cs));
}

float3 Blend(float3 cb, float3 cs, int mode) {
    if (mode == 1) { return Multiply(cb, cs); }
    if (mode == 2) { return Screen(cb, cs); }
    if (mode == 3) { return Overlay(cb, cs); }
    if (mode == 4) { return min(cb, cs); }              // Darken
    if (mode == 5) { return max(cb, cs); }               // Lighten
    if (mode == 6) { return ColorDodge(cb, cs); }
    if (mode == 7) { return ColorBurn(cb, cs); }
    if (mode == 8) { return HardLight(cb, cs); }
    if (mode == 9) { return SoftLight(cb, cs); }
    if (mode == 10) { return abs(cb - cs); }             // Difference
    if (mode == 11) { return cb + cs - 2.0 * cb * cs; }  // Exclusion
    return cs;                                           // Normal (mode == 0)
}

float4 PSMain(VSOutput input) : SV_Target {
    float4 src      = uSourceTexture.Sample(uSampler, input.Uv);
    float4 backdrop = uBackdropTexture.Sample(uSampler, input.Uv);

    float srcAlpha      = src.a;
    float backdropAlpha = backdrop.a;
    float3 cs = srcAlpha > 0.0 ? src.rgb / srcAlpha : float3(0.0, 0.0, 0.0);
    float3 cb = backdropAlpha > 0.0 ? backdrop.rgb / backdropAlpha : float3(0.0, 0.0, 0.0);

    float3 blended = Blend(cb, cs, Mode);

    float3 resultRgb = (1.0 - backdropAlpha) * srcAlpha * cs
                      + backdropAlpha * srcAlpha * blended
                      + (1.0 - srcAlpha) * backdropAlpha * cb;
    float resultAlpha = srcAlpha + backdropAlpha - srcAlpha * backdropAlpha;

    return float4(resultRgb, resultAlpha);
}
