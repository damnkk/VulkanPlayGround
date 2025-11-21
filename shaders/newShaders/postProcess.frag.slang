#include "common.slang"
struct FragmentInput
{
    float2 uv : TEXCOORD0;
};

struct FragmentOutput
{
    float4 color : SV_Target0;
};

// Per-Pass bindings
[vk::binding(0, 3)]
Texture2D<float4> inputTexture;

[shader("fragment")]
void main(FragmentInput fragIn, out FragmentOutput fragOut)
{
    fragOut.color = inputTexture.Sample(g_GlobalSampler_Linear, fragIn.uv);
}
