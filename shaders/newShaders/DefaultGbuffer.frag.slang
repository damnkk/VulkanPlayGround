#include "common.slang"
#include "tonemap_functions.h.slang"
#include "gltf_scene_io.h.slang"
#include "functions.h.slang"
#include "pbr_material_eval.h.slang"
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
    float4 emissive : SV_Target3;
    float4 custom : SV_Target4;
    float4 velocity : SV_Target5;
};

// Per-Pass bindings
[vk::binding(0, 3)]
Texture2D<float4> inputTexture;
[[vk::push_constant]]
ConstantBuffer<GBufferPushConstant> g_gBufferPushConstant;

// ============================================================================
// Shading Model Enum (与 GBufferConfig.h 对应)
// ============================================================================

static const uint SHADING_MODEL_DEFAULT_LIT = 0;
static const uint SHADING_MODEL_CLEARCOAT   = 1;
static const uint SHADING_MODEL_CLOTH       = 2;
static const uint SHADING_MODEL_SUBSURFACE  = 3;
static const uint SHADING_MODEL_IRIDESCENCE = 4;

// ============================================================================
// Octahedron Normal Encoding (高效法线压缩)
// ===============================================================
// 将单位法线编码为 2D ([-1,1]^3 -> [0,1]^2)
float2 encodeNormalOctahedron(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
    {
        float2 octWrap = (1.0 - abs(n.yx)) * float2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
        n.xy           = octWrap;
    }
    return n.xy * 0.5 + 0.5; // 映射到 [0, 1] 范围
}

// ============================================================================
// Shading Model 判断 (根据材质特性决定光照模型)
// ============================================================================

uint determineShadingModel(PbrMaterial mat)
{
    // 优先级：ClearCoat > Cloth > Subsurface > Iridescence > DefaultLit
    if (mat.clearcoat > 0.01)
        return SHADING_MODEL_CLEARCOAT;

    if (length(mat.sheenColor) > 0.01)
        return SHADING_MODEL_CLOTH;

    if (mat.transmission > 0.01)
        return SHADING_MODEL_SUBSURFACE;

    if (mat.iridescence > 0.01)
        return SHADING_MODEL_IRIDESCENCE;

    return SHADING_MODEL_DEFAULT_LIT;
}

// ============================================================================
// GBuffer 编码 (将 PbrMaterial 打包到 RT)
// ============================================================================

FragmentOutput encodeGBuffer(PbrMaterial mat, uint shadingModelID)
{
    FragmentOutput output;

    // RT0: BaseColor + AO
    output.albedo = float4(mat.baseColor, mat.occlusion);

    // RT1: WorldNormal (encoded) + Metallic + Opacity
    output.worldNormal = float4(encodeNormalOctahedron(mat.N), mat.metallic, mat.opacity);

    // RT2: Roughness (anisotropic) + Specular + padding
    output.pbr = float4(mat.roughness.x, mat.roughness.y, mat.specular, 0.0);

    // RT3: Emissive + ShadingModelID
    output.emissive = float4(mat.emissive, float(shadingModelID) / 255.0);

    // RT4: CustomData (根据 ShadingModel 动态填充)
    if (shadingModelID == SHADING_MODEL_CLEARCOAT)
    {
        // ClearCoat: clearcoat + clearcoatRoughness + clearcoatNormal (encoded)
        output.custom = float4(mat.clearcoat, mat.clearcoatRoughness, encodeNormalOctahedron(mat.Nc));
    }
    else if (shadingModelID == SHADING_MODEL_CLOTH)
    {
        // Cloth: sheenColor + sheenRoughness
        output.custom = float4(mat.sheenColor, mat.sheenRoughness);
    }
    else if (shadingModelID == SHADING_MODEL_SUBSURFACE)
    {
        // Subsurface: transmission + attenuationColor
        output.custom = float4(mat.transmission, mat.attenuationColor);
    }
    else if (shadingModelID == SHADING_MODEL_IRIDESCENCE)
    {
        // Iridescence: iridescence + iridescenceIor + iridescenceThickness
        output.custom = float4(mat.iridescence, mat.iridescenceIor, mat.iridescenceThickness, 0.0);
    }
    else
    {
        // DefaultLit: 未使用
        output.custom = float4(0.0);
    }

    // RT5: Velocity (暂时设为 0，后续实现 TAA 时计算)
    output.velocity = float4(0.0, 0.0, 0.0, 1.0);

    return output;
}

// PbrMaterial evaluateMaterial(in GltfShadeMaterial material, MeshState state, GltfTextureInfo* texInfos)
// {
//     // Material Evaluated
//     PbrMaterial pbrMat;

//     // pbrMetallicRoughness (standard)
//     if (material.usePbrSpecularGlossiness == 0)
//     {
//         // Base Color/Albedo may be defined from a base texture or a flat color
//         float4 baseColor = material.pbrBaseColorFactor;
//         if (isTexturePresent(material.pbrBaseColorTexture))
//         {
//             baseColor *= getTexture(textures, texInfos[material.pbrBaseColorTexture], state.tc);
//         }
//         pbrMat.baseColor = baseColor.rgb;
//         pbrMat.opacity   = baseColor.a;

//         // Metallic-Roughness
//         float roughness = material.pbrRoughnessFactor;
//         float metallic  = material.pbrMetallicFactor;
//         if (isTexturePresent(material.pbrMetallicRoughnessTexture))
//         {
//             // Roughness is stored in the 'g' channel, metallic is stored in the 'b' channel.
//             float4 metallicRoughnessSample = getTexture(textures, texInfos[material.pbrMetallicRoughnessTexture], state.tc);
//             roughness *= metallicRoughnessSample.g;
//             metallic *= metallicRoughnessSample.b;
//         }
//         roughness        = max(roughness, MICROFACET_MIN_ROUGHNESS);
//         pbrMat.roughness = float2(roughness * roughness); // Square roughness for the microfacet model
//         pbrMat.metallic  = clamp(metallic, 0.0F, 1.0F);
//     }
//     else
//     {
//         // KHR_materials_pbrSpecularGlossiness: deprecated but still used in many places
//         float4 diffuse    = material.pbrDiffuseFactor;
//         float  glossiness = material.pbrGlossinessFactor;
//         float3 specular   = material.pbrSpecularFactor;

//         if (isTexturePresent(material.pbrDiffuseTexture))
//         {
//             diffuse *= getTexture(textures, texInfos[material.pbrDiffuseTexture], state.tc);
//         }

//         if (isTexturePresent(material.pbrSpecularGlossinessTexture))
//         {
//             float4 specularGlossinessSample = getTexture(textures, texInfos[material.pbrSpecularGlossinessTexture], state.tc);
//             specular *= specularGlossinessSample.rgb;
//             glossiness *= specularGlossinessSample.a;
//         }

//         pbrMat.baseColor = convertSGToMR(diffuse.rgb, specular, glossiness, pbrMat.metallic, pbrMat.roughness);
//         pbrMat.opacity   = diffuse.a;
//     }

//     // Occlusion Map
//     pbrMat.occlusion = material.occlusionStrength;
//     if (isTexturePresent(material.occlusionTexture))
//     {
//         float occlusion  = getTexture(textures, texInfos[material.occlusionTexture], state.tc).r;
//         pbrMat.occlusion = 1.0 + pbrMat.occlusion * (occlusion - 1.0);
//     }

//     // Normal Map
//     pbrMat.N                = state.N;
//     pbrMat.T                = state.T;
//     pbrMat.B                = state.B;
//     pbrMat.Ng               = state.Ng;
//     bool needsTangentUpdate = false;

//     if (isTexturePresent(material.normalTexture))
//     {
//         float3 normal_vector = getTexture(textures, texInfos[material.normalTexture], state.tc).xyz;
//         normal_vector        = normal_vector * 2.0F - 1.0F;
//         normal_vector *= float3(material.normalTextureScale, material.normalTextureScale, 1.0F);
//         float3x3 tbn = float3x3(state.T, state.B, state.N);
//         pbrMat.N     = normalize(mul(normal_vector, tbn));

//         // Mark that we need to update T and B due to normal perturbation
//         needsTangentUpdate = true;
//     }

//     // Emissive term
//     pbrMat.emissive = material.emissiveFactor;
//     if (isTexturePresent(material.emissiveTexture))
//     {
//         pbrMat.emissive *= getTexture(textures, texInfos[material.emissiveTexture], state.tc).rgb;
//     }
//     pbrMat.emissive = max(float3(0.0F), pbrMat.emissive);

//     // KHR_materials_specular
//     // https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_specular
//     pbrMat.specularColor = material.specularColorFactor;
//     if (isTexturePresent(material.specularColorTexture))
//     {
//         pbrMat.specularColor *= getTexture(textures, texInfos[material.specularColorTexture], state.tc).rgb;
//     }

//     // KHR_materials_specular
//     pbrMat.specular = material.specularFactor;
//     if (isTexturePresent(material.specularTexture))
//     {
//         pbrMat.specular *= getTexture(textures, texInfos[material.specularTexture], state.tc).a;
//     }

//     // Dielectric Specular
//     float ior1 = 1.0F;                                    // IOR of the current medium (e.g., air)
//     float ior2 = material.ior;                            // IOR of the material
//     if (state.isInside && (material.thicknessFactor > 0)) // If the material is thin-walled, we don't need to consider the inside IOR.
//     {
//         ior1 = material.ior;
//         ior2 = 1.0F;
//     }
//     pbrMat.ior1 = ior1;
//     pbrMat.ior2 = ior2;

//     // KHR_materials_transmission
//     pbrMat.transmission = material.transmissionFactor;
//     if (isTexturePresent(material.transmissionTexture))
//     {
//         pbrMat.transmission *= getTexture(textures, texInfos[material.transmissionTexture], state.tc).r;
//     }

//     // KHR_materials_volume
//     pbrMat.attenuationColor    = material.attenuationColor;
//     pbrMat.attenuationDistance = material.attenuationDistance;
//     pbrMat.isThinWalled        = (material.thicknessFactor == 0.0);

//     // KHR_materials_clearcoat
//     pbrMat.clearcoat          = material.clearcoatFactor;
//     pbrMat.clearcoatRoughness = material.clearcoatRoughness;
//     pbrMat.Nc                 = pbrMat.N;
//     if (isTexturePresent(material.clearcoatTexture))
//     {
//         pbrMat.clearcoat *= getTexture(textures, texInfos[material.clearcoatTexture], state.tc).r;
//     }
//     if (isTexturePresent(material.clearcoatRoughnessTexture))
//     {
//         pbrMat.clearcoatRoughness *= getTexture(textures, texInfos[material.clearcoatRoughnessTexture], state.tc).g;
//     }
//     if (isTexturePresent(material.clearcoatNormalTexture))
//     {
//         float3x3 tbn           = float3x3(pbrMat.T, pbrMat.B, pbrMat.Nc);
//         float3   normal_vector = getTexture(textures, texInfos[material.clearcoatNormalTexture], state.tc).xyz;
//         normal_vector          = normal_vector * 2.0F - 1.0F;
//         pbrMat.Nc              = normalize(mul(normal_vector, tbn));
//     }
//     pbrMat.clearcoatRoughness = max(pbrMat.clearcoatRoughness, 0.001F);

//     // KHR_materials_iridescence
//     float iridescence          = material.iridescenceFactor;
//     float iridescenceThickness = material.iridescenceThicknessMaximum;
//     pbrMat.iridescenceIor      = material.iridescenceIor;
//     if (isTexturePresent(material.iridescenceTexture))
//     {
//         iridescence *= getTexture(textures, texInfos[material.iridescenceTexture], state.tc).x;
//     }
//     if (isTexturePresent(material.iridescenceThicknessTexture))
//     {
//         const float t        = getTexture(textures, texInfos[material.iridescenceThicknessTexture], state.tc).y;
//         iridescenceThickness = lerp(material.iridescenceThicknessMinimum, material.iridescenceThicknessMaximum, t);
//     }
//     pbrMat.iridescence          = (iridescenceThickness > 0.0f) ? iridescence : 0.0f; // No iridescence when the thickness is zero.
//     pbrMat.iridescenceThickness = iridescenceThickness;

//     // KHR_materials_anisotropy
//     float anisotropyStrength = material.anisotropyStrength;
//     // If the anisotropyStrength == 0.0f (default), the roughness is isotropic.
//     // No need to rotate the anisotropyDirection or tangent space.
//     if (anisotropyStrength > 0.0F)
//     {
//         float2 anisotropyDirection = float2(1.0f, 0.0f); // By default the anisotropy strength is along the tangent.
//         if (isTexturePresent(material.anisotropyTexture))
//         {
//             const float4 anisotropyTex = getTexture(textures, texInfos[material.anisotropyTexture], state.tc);

//             // .xy encodes the direction in (tangent, bitangent) space. Remap from [0, 1] to [-1, 1].
//             anisotropyDirection = normalize(float2(anisotropyTex.xy) * 2.0f - 1.0f);
//             // .z encodes the strength in range [0, 1].
//             anisotropyStrength *= anisotropyTex.z;
//         }

//         // Adjust the roughness to account for anisotropy.
//         pbrMat.roughness.x = lerp(pbrMat.roughness.y, 1.0f, anisotropyStrength * anisotropyStrength);

//         // Rotate the anisotropy direction in the tangent space.
//         const float s       = material.anisotropyRotation.x; // Sin and Cos of the rotation angle.
//         const float c       = material.anisotropyRotation.y;
//         anisotropyDirection = float2(c * anisotropyDirection.x + s * anisotropyDirection.y, c * anisotropyDirection.y - s * anisotropyDirection.x);

//         // Update the tangent to be along the anisotropy direction in tangent space.
//         const float3 T_aniso = pbrMat.T * anisotropyDirection.x + pbrMat.B * anisotropyDirection.y;

//         pbrMat.T           = T_aniso;
//         needsTangentUpdate = true;
//     }

//     // Perform tangent and bitangent updates if necessary
//     if (needsTangentUpdate)
//     {
//         // Ensure T, B, and N are orthonormal
//         pbrMat.B            = normalize(cross(pbrMat.N, pbrMat.T));
//         float bitangentSign = sign(dot(state.B, pbrMat.B));
//         pbrMat.B            = pbrMat.B * bitangentSign;
//         pbrMat.T            = normalize(cross(pbrMat.B, pbrMat.N) * bitangentSign);
//     }

//     // KHR_materials_sheen
//     pbrMat.sheenColor = material.sheenColorFactor;
//     if (isTexturePresent(material.sheenColorTexture))
//     {
//         pbrMat.sheenColor *= float3(getTexture(textures, texInfos[material.sheenColorTexture], state.tc).xyz); // sRGB
//     }

//     pbrMat.sheenRoughness = material.sheenRoughnessFactor;
//     if (isTexturePresent(material.sheenRoughnessTexture))
//     {
//         pbrMat.sheenRoughness *= getTexture(textures, texInfos[material.sheenRoughnessTexture], state.tc).w;
//     }
//     pbrMat.sheenRoughness = max(MICROFACET_MIN_ROUGHNESS, pbrMat.sheenRoughness);

//     // KHR_materials_dispersion
//     pbrMat.dispersion = material.dispersion;

//     // KHR_materials_diffuse_transmission
//     pbrMat.diffuseTransmissionFactor = material.diffuseTransmissionFactor;
//     if (isTexturePresent(material.diffuseTransmissionTexture))
//     {
//         pbrMat.diffuseTransmissionFactor *= getTexture(textures, texInfos[material.diffuseTransmissionTexture], state.tc).a;
//     }
//     pbrMat.diffuseTransmissionColor = material.diffuseTransmissionColor;
//     if (isTexturePresent(material.diffuseTransmissionColorTexture))
//     {
//         pbrMat.diffuseTransmissionColor = getTexture(textures, texInfos[material.diffuseTransmissionColorTexture], state.tc).rgb;
//     }

//     return pbrMat;
// }

[shader("fragment")]
FragmentOutput main(FragmentInput fragIn)
{
    GltfScene*        scene      = (GltfScene*) g_gBufferPushConstant.sceneConstant.sceneDescAddress;
    GltfRenderNode    renderNode = scene->renderNodes[g_gBufferPushConstant.sceneConstant.renderNodeId];
    GltfShadeMaterial material   = scene->materials[renderNode.materialID];

    // 方案 2：临时示例 - 创建一个默认材质用于测试
    PbrMaterial pbrMaterial = defaultPbrMaterial();
    pbrMaterial.baseColor   = float3(0.8, 0.8, 0.8);
    pbrMaterial.metallic    = 0.0;
    pbrMaterial.roughness   = float2(0.5);
    pbrMaterial.N           = normalize(fragIn.normal);
    orthonormalBasis(pbrMaterial.N, pbrMaterial.T, pbrMaterial.B);

    uint shadingModelID = determineShadingModel(pbrMaterial);
    return encodeGBuffer(pbrMaterial, shadingModelID);
}
