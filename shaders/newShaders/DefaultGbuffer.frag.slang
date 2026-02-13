#include "common.slang"
#include "tonemap_functions.h.slang"
#include "gltf_scene_io.h.slang"
#include "functions.h.slang"
struct FragmentInput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct FragmentOutput
{
    float4 albedo : SV_Target0;
    float4 worldNormal : SV_Target1;
    float4 pbr : SV_Target2;
    float4 velocity : SV_Target3;
    float4 custom : SV_Target4;
};

// Per-Pass bindings
[vk::binding(0, 3)]
Texture2D<float4> inputTexture;
[[vk::push_constant]]
ConstantBuffer<GBufferPushConstant> g_gBufferPushConstant;

[shader("fragment")]
FragmentOutput main(FragmentInput fragIn)
{
    GltfScene*        scene      = (GltfScene*) g_gBufferPushConstant.sceneConstant.sceneDescAddress;
    GltfRenderNode    renderNode = scene->renderNodes[g_gBufferPushConstant.sceneConstant.renderNodeId];
    GltfShadeMaterial material   = scene->materials[renderNode.materialID];
    float4            albedo     = material.pbrBaseColorFactor;
    if (material.pbrBaseColorTexture != 0)
    {
        GltfTextureInfo texInfo = scene->textureInfos[material.pbrBaseColorTexture];
        albedo = s_SceneTextures[texInfo.index + g_gBufferPushConstant.sceneConstant.textureOffset].SampleLevel(g_GlobalSampler_Linear, fragIn.uv, 0);
        albedo.xyz = toLinear(albedo.xyz);
    }
    float4 worldNormal = float4(0.0);
    if (material.normalTexture != 0)
    {
        GltfTextureInfo texInfo = scene->textureInfos[material.normalTexture];
        worldNormal =
            s_SceneTextures[texInfo.index + g_gBufferPushConstant.sceneConstant.textureOffset].SampleLevel(g_GlobalSampler_Linear, fragIn.uv, 0);
        worldNormal = normalize(worldNormal * 2.0 - 1.0);
        float3 tangent, bitangent;
        orthonormalBasis(fragIn.normal.xyz, tangent, bitangent);
        tangent         = normalize(tangent);
        bitangent       = normalize(bitangent);
        worldNormal.xyz = normalize(fragIn.normal.z * worldNormal.z + tangent * worldNormal.x + bitangent * worldNormal.y);
    }
    // metallic, roughness, occlusion, padding
    float4 pbr = float4(material.pbrMetallicFactor, material.pbrRoughnessFactor, material.occlusionStrength, 1.0);
    if (material.pbrMetallicRoughnessTexture != 0)
    {
        GltfTextureInfo texInfo = scene->textureInfos[material.pbrMetallicRoughnessTexture];
        pbr.xy *=
            s_SceneTextures[texInfo.index + g_gBufferPushConstant.sceneConstant.textureOffset].SampleLevel(g_GlobalSampler_Linear, fragIn.uv, 0).xy;
    }
    if (material.occlusionTexture != 0)
    {
        GltfTextureInfo texInfo = scene->textureInfos[material.occlusionTexture];
        pbr.z *=
            s_SceneTextures[texInfo.index + g_gBufferPushConstant.sceneConstant.textureOffset].SampleLevel(g_GlobalSampler_Linear, fragIn.uv, 0).r;
    }

    FragmentOutput fragOut;
    fragOut.albedo      = albedo;
    fragOut.worldNormal = worldNormal;
    fragOut.pbr         = pbr;
    fragOut.velocity    = float4(0.0, 1.0, 0.0, 1.0);
    fragOut.custom      = float4(1.0, 1.0, 0.0, 1.0);
    return fragOut;
}
