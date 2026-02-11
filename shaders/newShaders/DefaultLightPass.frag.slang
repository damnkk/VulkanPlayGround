#include "common.slang"
struct FragmentInput
{
    float2 uv : TEXCOORD0;
};

struct FragmentOutput
{
    float4 color : SV_Target0;
};

[vk_binding(0, 3)]
Texture2D<float4> inputAlbedo;
[vk_binding(1, 3)]
Texture2D<float4> inputNormal;
[vk_binding(2, 3)]
Texture2D<float4> inputPBR;
[vk_binding(3, 3)]
Texture2D<float4> inputCustom;
[vk_binding(4, 3)]
Texture2D<float4> inputVelocity;

[shader("fragment")]
void main(FragmentInput input, out FragmentOutput output)
{
    output.color = inputAlbedo.Sample(g_GlobalSampler_Linear, input.uv);
}
