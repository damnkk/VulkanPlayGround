#include "GBufferCommon.h.slang"

struct FragmentOutput
{
    float4 albedo : SV_Target0;
    float4 worldNormal : SV_Target1;
    float4 pbr : SV_Target2;
    float4 emissive : SV_Target3;
    float4 custom : SV_Target4;
    float4 velocity : SV_Target5;
};

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

float4 sampleMaterialTexture(GpuSceneModelData model, uint textureInfoIndex, GBufferVertexOutput fragIn, float4 fallback)
{
    ModelTextureInfo textureInfo;
    if (!loadGpuSceneTextureInfo(model, textureInfoIndex, textureInfo)) return fallback;
    return sampleGpuSceneTexture(model, textureInfo, fragIn.uv, fragIn.uv1);
}

[shader("fragment")]
FragmentOutput main(GBufferVertexOutput fragIn)
{
    GpuSceneData scene = loadGpuScene(g_gBufferPushConstant.sceneConstant.sceneAddress);
    GpuSceneDrawData draw = loadGpuSceneDraw(scene, fragIn.drawID);
    GpuSceneModelData model = loadGpuSceneModel(scene, draw.modelID);
    ModelDrawableInfo drawable = loadGpuSceneDrawable(model, draw.drawableID);
    GltfShadeMaterial material = loadGpuSceneMaterial(model, drawable);

    float4 baseColor = material.pbrBaseColorFactor * sampleMaterialTexture(model, material.pbrBaseColorTexture, fragIn, float4(1.0));

    if (material.alphaMode == AlphaMode::eAlphaModeMask && baseColor.a < material.alphaCutoff) discard;

    float           roughness = material.pbrRoughnessFactor;
    float           metallic  = material.pbrMetallicFactor;
    float           specular  = material.specularFactor;
    ModelTextureInfo metallicRoughnessTextureInfo;
    if (loadGpuSceneTextureInfo(model, material.pbrMetallicRoughnessTexture, metallicRoughnessTextureInfo))
    {
        float4 metallicRoughnessSample = sampleGpuSceneTexture(model, metallicRoughnessTextureInfo, fragIn.uv, fragIn.uv1);
        roughness *= metallicRoughnessSample.g;
        metallic *= metallicRoughnessSample.b;
    }

    ModelTextureInfo specularTextureInfo;
    if (loadGpuSceneTextureInfo(model, material.specularTexture, specularTextureInfo))
    {
        specular *= sampleGpuSceneTexture(model, specularTextureInfo, fragIn.uv, fragIn.uv1).a;
    }

    float           occlusion = material.occlusionStrength;
    ModelTextureInfo occlusionTextureInfo;
    if (loadGpuSceneTextureInfo(model, material.occlusionTexture, occlusionTextureInfo))
    {
        float occlusionSample = sampleGpuSceneTexture(model, occlusionTextureInfo, fragIn.uv, fragIn.uv1).r;
        occlusion = 1.0 + material.occlusionStrength * (occlusionSample - 1.0);
    }

    float3 emissive = material.emissiveFactor * sampleMaterialTexture(model, material.emissiveTexture, fragIn, float4(1.0)).rgb;

    float3 normal             = normalize(fragIn.normal);
    float3 tangent            = normalize(fragIn.tangent.xyz);
    tangent                   = normalize(tangent - normal * dot(normal, tangent));
    float3          bitangent = normalize(cross(normal, tangent) * fragIn.tangent.w);
    ModelTextureInfo normalTextureInfo;
    if (loadGpuSceneTextureInfo(model, material.normalTexture, normalTextureInfo))
    {
        float3 tangentNormal = sampleGpuSceneTexture(model, normalTextureInfo, fragIn.uv, fragIn.uv1).xyz * 2.0 - 1.0;
        tangentNormal.xy *= material.normalTextureScale;
        normal = normalize(tangentNormal.x * tangent + tangentNormal.y * bitangent + tangentNormal.z * normal);
    }
    float2 encodedNormal = encodeOctahedron(normal);

    FragmentOutput output;
    output.albedo      = float4(baseColor.rgb, occlusion);
    output.worldNormal = float4(encodedNormal, metallic, baseColor.a);
    output.pbr         = float4(roughness, roughness, specular, 0.0);
    output.emissive    = float4(emissive, 0.0);
    output.custom      = fragIn.customData;
    output.velocity    = float4(0.0, 0.0, 0.0, 0.0);
    return output;
}
