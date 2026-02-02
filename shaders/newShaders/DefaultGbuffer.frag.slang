#include "gltf_scene_io.h.slang"
#include "PConstantType.h.slang"

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
void main(FragmentInput fin, out FragmentOutput fout)
{
    float3   albedo         = float3(1.0, 0.0, 1.0);
    float    metallic       = 0.0;
    float    roughness      = 1.0;
    float    ao             = 1.0;
    float3   worldNormal    = normalize(fin.normal);
    float3   worldTangent   = normalize(fin.tangent);
    float3   worldBitangent = normalize(cross(worldNormal, worldTangent));
    float3x3 TBN            = float3x3(worldTangent, worldBitangent, worldNormal);

    fout.outAlbedo        = float4(albedo, 1.0);
    fout.outWorldNormal   = float4(worldNormal * 0.5 + 0.5, 1.0);
    fout.outPBR           = float4(metallic, roughness, ao, 1.0);
    fout.outCustom        = float4(0.0, 0.0, 0.0, 1.0);
    fout.GBuffer_Velocity = float4(0.0, 0.0, 0.0, 1.0);
}
