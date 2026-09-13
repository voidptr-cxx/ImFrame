// GaussianBlur.glsl — separable Gaussian blur, one compute dispatch per axis.
//
// The Vulkan/compute counterpart to `BlurPassGL3.cpp`'s fragment-shader ping-pong pass (Phase
// 34.3) — see that file's own header comment for why kernel weights are computed inline via
// exp() rather than precomputed into a UBO array the way PHASE_34_PROPOSAL.md's original
// compute-shader section describes: the same reasoning applies here (no measured performance
// problem motivates the added machinery, and per-invocation exp() is trivial), and using the
// identical formula keeps GL3's and Vulkan's blur output visually consistent. One compute
// pipeline handles both the horizontal and vertical pass, selected per-dispatch by `Direction`
// in the push-constant block — mirroring `BlurPassGL3`'s own single-program-both-directions
// design (`uDirection` there, `Direction` here).
//
// `imageLoad`/`imageStore` (not `texture()`) — direct storage-image access has no sampler, so
// there is no hardware CLAMP_TO_EDGE; out-of-bounds taps are clamped manually via `clamp()` on
// the sample coordinate instead, achieving the identical edge behaviour `BlurPassGL3`'s
// `GL_CLAMP_TO_EDGE` texture parameter gives it.

#version 450

#if defined(STAGE_COMPUTE)

layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0, rgba8) uniform readonly image2D uSource;
layout(binding = 1, rgba8) uniform writeonly image2D uDest;

layout(push_constant) uniform PushConstants {
    vec2  Direction; // (1,0) horizontal pass, (0,1) vertical pass
    float Radius;    // pre-clamped to [0, 64] on the CPU side
} pc;

void main() {
    ivec2 size  = imageSize(uSource);
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    // The dispatch grid is rounded up to a whole number of 16x16 workgroups, so invocations at
    // the right/bottom edge can fall outside the actual image -- discard them.
    if (coord.x >= size.x || coord.y >= size.y) { return; }

    int   radius     = int(pc.Radius);
    float sigma      = max(pc.Radius * 0.5, 1e-4);
    float twoSigmaSq = 2.0 * sigma * sigma;

    vec4  sum       = vec4(0.0);
    float weightSum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        float weight       = exp(-float(i * i) / twoSigmaSq);
        ivec2 offset       = ivec2(pc.Direction * float(i));
        ivec2 sampleCoord  = clamp(coord + offset, ivec2(0), size - ivec2(1));
        sum += imageLoad(uSource, sampleCoord) * weight;
        weightSum += weight;
    }
    imageStore(uDest, coord, sum / weightSum);
}

#else
#error "GaussianBlur.glsl must be compiled with -DSTAGE_COMPUTE"
#endif
