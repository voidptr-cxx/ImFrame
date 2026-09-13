// Image.hlsl — DX12 port of Shaders/Image.glsl (see that file's own header comment for the
// rounded-box SDF math and fwidth-derived AA band derivation — both unchanged here, ported
// directly from GLSL to HLSL syntax, mirroring SDFRect.hlsl's own identical porting approach).
//
// Y IS negated in VSMain, matching SDFRect.hlsl's own D3D convention (see that file's comment for
// the full Vulkan-vs-D3D NDC-handedness derivation — the same reasoning applies here).
//
// The texture is bound as a real Texture2D/SamplerState pair (register t0/s0) rather than a single
// combined-image-sampler the way Image.glsl's GLSL binds it — HLSL has no combined-sampler type,
// so NativeRendererDX12 binds a descriptor-table SRV (t0) alongside a fixed root-signature static
// sampler (s0), the closest D3D12 analogue to NativeRendererVulkan's own single shared VkSampler.
//
// Compiled twice by cmake/CompileShaderDXC.cmake's compile_shader_hlsl() — once as `vs_6_0`
// targeting VSMain, once as `ps_6_0` targeting PSMain — matching SDFRect.hlsl's own convention.

cbuffer PerFrame : register(b0) {
    float2 ViewportSize;
};

Texture2D    uTexture : register(t0);
SamplerState uSampler : register(s0);

struct VSInput {
    float2 Position  : POSITION;   // pixel-space vertex position
    float2 Local     : LOCAL;      // position relative to the rect's center, in pixels
    float2 HalfSize  : HALFSIZE;   // rect half-width/half-height, in pixels
    float4 Radii     : RADII;      // per-corner radii: x=TopLeft, y=TopRight, z=BottomRight, w=BottomLeft
    float2 Uv        : UV;
    float4 TintColor : TINTCOLOR;
};

struct PSInput {
    float4 ClipPosition : SV_Position;
    float2 Local        : LOCAL;
    float2 HalfSize     : HALFSIZE;
    float4 Radii        : RADII;
    float2 Uv           : UV;
    float4 TintColor    : TINTCOLOR;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    float2 ndc = (input.Position / ViewportSize) * 2.0 - 1.0;
    output.ClipPosition = float4(ndc.x, -ndc.y, 0.0, 1.0);
    output.Local        = input.Local;
    output.HalfSize      = input.HalfSize;
    output.Radii        = input.Radii;
    output.Uv           = input.Uv;
    output.TintColor    = input.TintColor;
    return output;
}

// Identical to SDFRect.hlsl's own RoundedBoxSdf() -- duplicated rather than shared via #include,
// matching Image.glsl's own file comment on why (no cross-shader #include convention exists yet
// in this codebase; keep both copies in sync by hand if the formula ever changes).
float RoundedBoxSdf(float2 p, float2 b, float4 r) {
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x  = (p.y > 0.0) ? r.x : r.y;
    float2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, float2(0.0, 0.0))) - r.x;
}

float4 PSMain(PSInput input) : SV_Target {
    float4 r = float4(input.Radii.y, input.Radii.z, input.Radii.x, input.Radii.w);
    float dist = RoundedBoxSdf(input.Local, input.HalfSize, r);

    float aa   = max(fwidth(dist) * 0.5, 1e-4);
    float mask = 1.0 - smoothstep(-aa, aa, dist);
    return uTexture.Sample(uSampler, input.Uv) * input.TintColor * mask;
}
