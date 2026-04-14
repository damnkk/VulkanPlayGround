#include "common.slang"
#include "constants.h.slang"

[vk::binding(0, 3)]
RWTexture2D<float4> g_TransmittanceLutOutput;

[vk::binding(1, 3)]
ConstantBuffer<AtmosParameter> u_atmosParameter;

[numthreads(8, 8, 1)]
[shader("compute")]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint width;
    uint height;
    g_TransmittanceLutOutput.GetDimensions(width, height);
    if (dispatchThreadId.x >= width || dispatchThreadId.y >= height) return;

    g_TransmittanceLutOutput[dispatchThreadId.xy] = float4(1.0, 0.0, 0.0, 1.0);
}
