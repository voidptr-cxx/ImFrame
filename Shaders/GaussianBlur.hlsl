// GaussianBlur.hlsl — DX12 port of Shaders/GaussianBlur.glsl (see that file's own header comment
// for the separable-blur/inline-exp()-weight derivation — unchanged here, ported directly from
// GLSL to HLSL syntax, mirroring SDFRect.hlsl's/Image.hlsl's own porting approach).
//
// Both uSource and uDest are RWTexture2D (UAV), matching GaussianBlur.glsl's own choice of a
// storage image (not a sampled texture) for the source too -- no hardware CLAMP_TO_EDGE exists
// for a UAV any more than it did for Vulkan's imageLoad, so out-of-bounds taps are still clamped
// manually via clamp() on the sample coordinate.
//
// Direction/Radius are passed as root constants (SetComputeRoot32BitConstants), the D3D12
// analogue of GaussianBlur.glsl's own push-constant block -- no CBV/root-CBV needed for three
// scalars that change every dispatch.
//
// Compiled once, as cs_6_0 targeting CSMain, by cmake/CompileShaderDXC.cmake's own
// compile_shader_hlsl() (extended this phase to accept a "cs_6_0" profile alongside its existing
// vs_6_0/ps_6_0 support).

RWTexture2D<float4> uSource : register(u0);
RWTexture2D<float4> uDest   : register(u1);

// Backed by root constants (SetComputeRoot32BitConstants), not a real constant buffer -- whether
// b0 is a root CBV or root constants is decided entirely by the root signature, not this
// declaration, so this reads like any other cbuffer.
cbuffer PushConstants : register(b0) {
    float2 Direction; // (1,0) horizontal pass, (0,1) vertical pass
    float  Radius;    // pre-clamped to [0, 64] on the CPU side
};

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint width, height;
    uSource.GetDimensions(width, height);

    int2 size  = int2(width, height);
    int2 coord = int2(dispatchThreadId.xy);
    // The dispatch grid is rounded up to a whole number of 16x16 groups, so threads at the
    // right/bottom edge can fall outside the actual image -- discard them.
    if (coord.x >= size.x || coord.y >= size.y) { return; }

    int   radius     = int(Radius);
    float sigma      = max(Radius * 0.5, 1e-4);
    float twoSigmaSq = 2.0 * sigma * sigma;

    float4 sum       = float4(0.0, 0.0, 0.0, 0.0);
    float  weightSum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        float weight      = exp(-float(i * i) / twoSigmaSq);
        int2  offset      = int2(Direction * float(i));
        int2  sampleCoord = clamp(coord + offset, int2(0, 0), size - int2(1, 1));
        sum += uSource[sampleCoord] * weight;
        weightSum += weight;
    }
    uDest[coord] = sum / weightSum;
}
