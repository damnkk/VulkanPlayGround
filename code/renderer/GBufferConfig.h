#ifndef GBUFFER_CONFIG_H
#define GBUFFER_CONFIG_H
#include <vulkan/vulkan.h>

namespace Play
{

/*
 * 弹性 GBuffer 配置（Dynamic Shading Model）:
 * ---------------------------------------------------------
 * 借鉴 UE5 设计：使用 ShadingModelID 动态解释 GCustomData 内容
 * 优势：显存高效（仅 5 个彩色 RT），支持所有 nvpro PbrMaterial 高级特性
 * 策略：全管线 16-bit Float，4090D 显卡保证精度无损
 *
 * === 固定布局（所有材质共享）===
 *
 * RT0: GBaseColor - BaseColor & AO
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Channels: RGB = baseColor (linear), A = AO/Occlusion
 *
 * RT1: GNormal - WorldNormal & Metallic
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Channels: RG = Normal.xy (Octahedron encode), B = metallic, A = opacity
 * - Note: Normal.z 从 xy 解码; T, B 通过 orthonormalBasis(N) 恢复
 *
 * RT2: GPBR - Roughness & Specular
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Channels: R = roughness.x (tangent), G = roughness.y (bitangent), B = specular, A = padding
 *
 * RT3: GEmissive - Emissive & ShadingModelID
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Channels: RGB = emissive (HDR), A = ShadingModelID (as float)
 *
 * === 弹性布局（根据 ShadingModelID 动态解释）===
 *
 * RT4: GCustomData - 自定义数据槽
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Channels: **根据 ShadingModelID 动态解释**（见下方表格）
 *
 * | ShadingModel | CustomData.r | CustomData.g | CustomData.b | CustomData.a |
 * |--------------|--------------|--------------|--------------|---------------|
 * | DefaultLit   | (unused)     | (unused)     | (unused)     | (unused)      |
 * | ClearCoat    | clearcoat    | clearcoatRoughness | ClearCoatNormal.x | ClearCoatNormal.y |
 * | Cloth        | sheenColor.r | sheenColor.g | sheenColor.b | sheenRoughness |
 * | Subsurface   | transmission | attenuationColor.r | attenuationColor.g | attenuationColor.b |
 * | Iridescence  | iridescence  | iridescenceIor | iridescenceThickness | (unused) |
 *
 * === 辅助数据 ===
 *
 * RT5: GVelocity - Motion Vectors
 * - Format: VK_FORMAT_R16G16_SFLOAT
 * - Channels: RG = screenSpaceVelocity (用于TAA)
 *
 * Depth: GSceneDepth
 * - Format: VK_FORMAT_D32_SFLOAT_S8_UINT
 *
 * === 带宽估算（4K分辨率）===
 * - 5个RT @ R16G16B16A16 + 1个R16G16 + 1个D32S8 ≈ 336 MB (绑定)/672 MB (往返)
 * - 相比完全分离方案节省 50% 显存
 */

// Shading Model 枚举 - 决定 GCustomData 的解释方式
enum class ShadingModel : uint32_t
{
    DefaultLit  = 0, // 标准 PBR（BaseColor + Normal + Metallic + Roughness）
    ClearCoat   = 1, // 车漆/涂层（增加 ClearCoat 层）
    Cloth       = 2, // 布料/丝绸（Sheen 效果）
    Subsurface  = 3, // 次表面/透射（Transmission + Attenuation）
    Iridescence = 4, // 彩虹色/肥皂泡（Iridescence）
    Count
};

// GBuffer Attachment 索引定义 - 弹性 RT 布局
enum class GBufferType : int
{
    GBaseColor = 0, // RT0: BaseColor + AO
    GNormal,        // RT1: WorldNormal (encoded) + Metallic + Opacity
    GPBR,           // RT2: Roughness (anisotropic) + Specular
    GEmissive,      // RT3: Emissive (HDR) + ShadingModelID
    GCustomData,    // RT4: 弹性槽位（根据 ShadingModelID 动态解释）
    GVelocity,      // RT5: Motion Vectors
    GSceneDepth,    // Depth/Stencil (separate attachment)
    Count           // 计数
};

struct GBufferRTParam
{
    GBufferType       type;
    const char*       debugName;
    VkFormat          format;
    VkImageUsageFlags usage;

    bool IsDepth() const
    {
        return type == GBufferType::GSceneDepth;
    }
};

struct GBufferConfig
{
    static constexpr int RT_COUNT = static_cast<int>(GBufferType::GSceneDepth); // 不包含 Depth 的 Color RT 数量

    // 获取指定 Attachment 的配置
    static GBufferRTParam Get(GBufferType type)
    {
        VkImageUsageFlags colorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        VkImageUsageFlags depthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        switch (type)
        {
            case GBufferType::GBaseColor:
                return {type, "GBuffer_BaseColor_AO", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GNormal:
                return {type, "GBuffer_Normal_Metallic_Opacity", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GPBR:
                return {type, "GBuffer_Roughness_Specular", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GEmissive:
                return {type, "GBuffer_Emissive_ShadingModelID", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GCustomData:
                return {type, "GBuffer_CustomData_Dynamic", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GVelocity:
                return {type, "GBuffer_Velocity", VK_FORMAT_R16G16_SFLOAT, colorUsage};
            case GBufferType::GSceneDepth:
                return {type, "SceneDepth", VK_FORMAT_D32_SFLOAT_S8_UINT, depthUsage};
            default:
                return {GBufferType::Count, "Invalid", VK_FORMAT_UNDEFINED, 0};
        }
    }

    // 辅助函数：将 ShadingModel 编码到 float
    static float EncodeShadingModel(ShadingModel model)
    {
        return static_cast<float>(model) / 255.0f;
    }

    // 辅助函数：从 float 解码 ShadingModel
    static ShadingModel DecodeShadingModel(float encoded)
    {
        return static_cast<ShadingModel>(static_cast<uint32_t>(encoded * 255.0f + 0.5f));
    }
};

} // namespace Play
#endif // GBUFFER_CONFIG_H