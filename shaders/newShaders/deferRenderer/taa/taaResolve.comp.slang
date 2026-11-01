#include "common.slang"
#include "PConstantType.h.slang"
#include "taaCommon.h.slang"

[[vk::push_constant]]
ConstantBuffer<TAAPushConstant> g_TaaPushConstant;

[[vk::binding(0, 3)]]
Texture2D<float4> g_CurrentColor; // Scene-linear HDR; compress samples before resolving.
[[vk::binding(1, 3)]]
Texture2D<float2> g_Velocity;
[[vk::binding(2, 3)]]
Texture2D<float4> g_HistoryColor;
[[vk::binding(3, 3)]]
RWTexture2D<float4> g_ResolvedColor;

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    uint width;
    uint height;
    g_CurrentColor.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
        return;

    int2   pixel = int2(dispatchThreadID.xy);
    float2 uv    = (float2(pixel) + 0.5) / float2(width, height);

    float3 current  = Tonemap(g_CurrentColor.Load(int3(pixel, 0)).rgb);
    float3 histroy  = Tonemap(g_HistoryColor.Load(int3(pixel, 0)).rgb);
    float3 resolved = lerp(histroy, current, 0.05f);

    // TODO: Implement history reprojection and temporal blending here.
    // currentWeight == 1 means history is not initialized; use current only.
    // Both current and stored history use the compressed color space.
    // Motion is unjittered; sample it at the original raster UV/pixel.
    // jitterUV / previousJitterUV are UV offsets, not NDC offsets.
    // Decide whether your resolved history uses a fixed grid or retains jitter
    // before choosing its lookup coordinates.

    g_ResolvedColor[pixel] = float4(resolved, 1.0);
}
