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
