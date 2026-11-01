#include "common.slang"
#include "taaCommon.h.slang"

[[vk::binding(0, 3)]]
Texture2D<float4> g_InputColor; // Resolved color in tonemapped space.
[[vk::binding(1, 3)]]
RWTexture2D<float4> g_OutputColor;
[[vk::binding(2, 3)]]
RWTexture2D<float4> g_HistoryColor;

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint width;
    uint height;
    g_InputColor.GetDimensions(width, height);
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height) return;

    int2   pixel    = int2(dispatchThreadID.xy);
    float4 resolved = g_InputColor.Load(int3(pixel, 0));

    g_OutputColor[pixel] = float4(InverseTonemap(resolved.rgb), resolved.a);
    // Store next frame's history here instead of copying it in a separate pass.
    g_HistoryColor[pixel] = resolved;
}
