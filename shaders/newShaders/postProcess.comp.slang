#include "common.slang"
#include "tonemap_functions.h.slang"

// Per-Pass bindings
[vk::binding(0, 3)]
Texture2D<float4> inputTexture;
[vk::binding(1, 3)]
RWTexture2D<float4> outputTexture;

[numthreads(16, 16, 1)]
[shader("compute")]
void main(uint3 sv_dispatchThreadID: SV_DispatchThreadID)
{
    float exposure = g_TonemapperDataBuffer.exposure;
    // TonemapperData tonemapperData         = g_TonemapperDataBuffer;
    outputTexture[sv_dispatchThreadID.xy] = float4(1.0, exposure, 0.0, 1.0);

    // fragOut.color.xyz = toSrgb(fragOut.color.xyz);
    // fragOut.color.xyz =
}
