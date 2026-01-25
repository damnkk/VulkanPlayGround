#include "gltf_scene_io.h.slang"
#include "PConstantType.h.slang"
struct GBufferPushConstant
{
    SceneConstant    sceneConstant;
    PerFrameConstant perFrameConstant;
};
[[vk::push_constant]]
ConstantBuffer<GBufferPushConstant> g_PerFrameConstant;
struct FragmentInput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};
struct FragmentOutput
{
    float4 outAlbedo : SV_Target0;
    float4 outWorldNormal : SV_Target1;
    float4 outPBR : SV_Target2;
    float4 outCustom : SV_Target3;
    float4 GBuffer_Velocity : SV_Target4;
};
[shader("fragment")]
void main(FragmentInput fin, out FragmentOutput fout) {}
