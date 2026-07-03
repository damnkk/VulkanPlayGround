#include "common.slang"
#include "GBufferCommon.h.slang"

struct FragmentInput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct FragmentOutput
{
    float4 albedo : SV_Target0;
    float4 worldNormal : SV_Target1;
    float4 pbr : SV_Target2;
    float4 emissive : SV_Target3;
    float4 custom : SV_Target4;
    float4 velocity : SV_Target5;
};

[[vk::push_constant]]
ConstantBuffer<GBufferPushConstant> g_gBufferPushConstant;

float2 encodeOctahedron(float3 normal)
{
    float3 n = normal / (abs(normal.x) + abs(normal.y) + abs(normal.z));
    if (n.z < 0.0)
    {
        float2 signNotZero = float2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
        n.xy               = (1.0 - abs(n.yx)) * signNotZero;
    }
    return n.xy * 0.5 + 0.5;
}

GBufferGPUInstanceData loadInstance()
{
    GBufferGPUInstanceData instance;
    instance.materialAddress    = 0;
    instance.textureInfoAddress = 0;
    instance.textureOffset      = 0;
    instance.textureInfoCount   = 0;
    instance.textureCount       = 0;

    uint64_t instanceBufferAddress = g_gBufferPushConstant.sceneConstant.instanceBufferAddress;
    if (instanceBufferAddress == 0)
    {
        return instance;
    }

    GBufferGPUInstanceData* instances = (GBufferGPUInstanceData*) instanceBufferAddress;
    return instances[uint(g_gBufferPushConstant.sceneConstant.instanceIndex)];
}

GltfShadeMaterial loadMaterial(GBufferGPUInstanceData instance)
{
    GltfShadeMaterial material = defaultGltfMaterial();
    if (instance.materialAddress != 0)
    {
        material = ((GltfShadeMaterial*) instance.materialAddress)[0];
    }
    return material;
}

bool loadTextureInfo(GBufferGPUInstanceData instance, uint textureInfoIndex, out GltfTextureInfo textureInfo)
{
    textureInfo = defaultGltfTextureInfo();
    if (textureInfoIndex == 0 || textureInfoIndex > instance.textureInfoCount || instance.textureInfoAddress == 0)
    {
        return false;
    }

    GltfTextureInfo* textureInfos = (GltfTextureInfo*) instance.textureInfoAddress;
    textureInfo                   = textureInfos[textureInfoIndex];
    return textureInfo.index >= 0 && uint(textureInfo.index) < instance.textureCount;
}

float2 selectTexCoord(FragmentInput fragIn, GltfTextureInfo textureInfo)
{
    return textureInfo.texCoord == 1 ? fragIn.uv1 : fragIn.uv;
}

float2 transformTexCoord(FragmentInput fragIn, GltfTextureInfo textureInfo)
{
    return mul(float3(selectTexCoord(fragIn, textureInfo), 1.0), textureInfo.uvTransform).xy;
}

float4 sampleSceneTexture(GBufferGPUInstanceData instance, GltfTextureInfo textureInfo, FragmentInput fragIn)
{
    return s_SceneTextures[instance.textureOffset + uint(textureInfo.index)].Sample(g_GlobalSampler_Linear,
                                                                                   transformTexCoord(fragIn, textureInfo));
}

float4 sampleMaterialTexture(GBufferGPUInstanceData instance, uint textureInfoIndex, FragmentInput fragIn, float4 fallback)
{
    GltfTextureInfo textureInfo;
    if (!loadTextureInfo(instance, textureInfoIndex, textureInfo))
    {
        return fallback;
    }

    return sampleSceneTexture(instance, textureInfo, fragIn);
}

[shader("fragment")]
FragmentOutput main(FragmentInput fragIn)
{
    GBufferGPUInstanceData instance = loadInstance();
    GltfShadeMaterial      material = loadMaterial(instance);

    float4 baseColor = material.pbrBaseColorFactor * sampleMaterialTexture(instance, material.pbrBaseColorTexture, fragIn, float4(1.0));

    float           roughness = material.pbrRoughnessFactor;
    float           metallic  = material.pbrMetallicFactor;
    float           specular  = material.specularFactor;
    GltfTextureInfo metallicRoughnessTextureInfo;
    if (loadTextureInfo(instance, material.pbrMetallicRoughnessTexture, metallicRoughnessTextureInfo))
    {
        float4 metallicRoughnessSample = sampleSceneTexture(instance, metallicRoughnessTextureInfo, fragIn);
        roughness *= metallicRoughnessSample.g;
        metallic *= metallicRoughnessSample.b;
    }

    GltfTextureInfo specularTextureInfo;
    if (loadTextureInfo(instance, material.specularTexture, specularTextureInfo))
    {
        specular *= sampleSceneTexture(instance, specularTextureInfo, fragIn).a;
    }

    float           occlusion = material.occlusionStrength;
    GltfTextureInfo occlusionTextureInfo;
    if (loadTextureInfo(instance, material.occlusionTexture, occlusionTextureInfo))
    {
        float occlusionSample = sampleSceneTexture(instance, occlusionTextureInfo, fragIn).r;
        occlusion = 1.0 + material.occlusionStrength * (occlusionSample - 1.0);
    }

    float3 emissive = material.emissiveFactor * sampleMaterialTexture(instance, material.emissiveTexture, fragIn, float4(1.0)).rgb;

    float3 normal             = normalize(fragIn.normal);
    float3 tangent            = normalize(fragIn.tangent.xyz);
    tangent                   = normalize(tangent - normal * dot(normal, tangent));
    float3          bitangent = normalize(cross(normal, tangent) * fragIn.tangent.w);
    GltfTextureInfo normalTextureInfo;
    if (loadTextureInfo(instance, material.normalTexture, normalTextureInfo))
    {
        float3 tangentNormal = sampleSceneTexture(instance, normalTextureInfo, fragIn).xyz * 2.0 - 1.0;
        tangentNormal.xy *= material.normalTextureScale;
        normal = normalize(tangentNormal.x * tangent + tangentNormal.y * bitangent + tangentNormal.z * normal);
    }
    float2 encodedNormal = encodeOctahedron(normal);

    FragmentOutput output;
    output.albedo      = float4(baseColor.rgb, occlusion);
    output.worldNormal = float4(encodedNormal, metallic, baseColor.a);
    output.pbr         = float4(roughness, roughness, specular, 0.0);
    output.emissive    = float4(emissive, 0.0);
    output.custom      = float4(0.0, 0.0, 0.0, 0.0);
    output.velocity    = float4(0.0, 0.0, 0.0, 0.0);
    return output;
}
