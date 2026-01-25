#ifndef GBUFFER_CONFIG_H
#define GBUFFER_CONFIG_H
#include <vulkan/vulkan.h>

namespace Play
{

/*
 * 高精度 GBuffer 配置(High Precision / Cinematic Profile):
 * ---------------------------------------------------------
 * 目标: 极致画质，无视显存与带宽开销 (适用于 24G VRAM+ 高端硬件)
 * 策略: 全管线采用 16-bit Floating Point (Half) 或更高，彻底消除色带(Banding)和精度误差。
 * 这里的原则是：除了 Swapchain Present 是 8-bit，中间计算全部保持至少 16-bit 浮点精度。
 *
 * RT0: GBufferA (BaseColor / Albedo)
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Util: 即使 BaseColor 通常在 [0,1]，16F 允许存储超高动态范围的 Emissive 或保留极高精度的线性颜色数据，
 *         避免多次伽马校正带来的累计误差。
 * - Channels:
 *   - RGB: Base Color (High Precision Linear)
 *   - A:   PerObject Data / AO (16-bit precision)
 *
 * RT1: GBufferB (WorldNormal)
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Util: 相比 10bit，16F 法线能完美表达微表面和高精模型细节，彻底消除几何边缘和高光处的锯齿与量化噪声。
 *         不再需要复杂的法线压缩编码。
 * - Channels:
 *   - RGB: World Space Normal (直接存储 [-1, 1] 或 [0, 1])
 *   - A:   Subsurface / Anisotropy Data
 *
 * RT2: GBufferC (Material Attributes / PBR)
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Util: Roughness 和 Metallic 在极低或极高值时，8-bit 可能导致非物理的截断，16F 保证光照计算的连续性。
 * - Channels:
 *   - R: Metallic
 *   - G: Specular
 *   - B: Roughness
 *   - A: Shading Model ID (虽然ID是整数，但作为Float存储也没问题，或用于混合权重)
 *
 * RT3: GBufferD (Custom Data) - 可选
 * - Format: VK_FORMAT_R16G16B16A16_SFLOAT
 * - Channels:
 *   - RGBA: High Precision Custom Data (ClearCoat Normal, Transmission, Skin Profile 等)
 *
 * RT4: SceneDepth (Depth/Stencil)
 * - Format: VK_FORMAT_D32_SFLOAT_S8_UINT
 * - Usage: 保持 32-bit 浮点深度以配合高精度管线。
 *
 * Extra RT: Velocity (Motion Vectors)
 * - Format: VK_FORMAT_R16G16_SFLOAT (最低要求) 或 VK_FORMAT_R32G32_SFLOAT (为了极致精度)
 * - Channels:
 *   - RG: Screen Space Velocity
 */

// GBuffer Attachment 索引定义
enum class GBufferType : int
{
    GBaseColor = 0, // BaseColor
    GWorldNormal,   // WorldNormal
    GPBR,           // PBR
    GVelocity,      // Motion Objects
    GSceneDepth,    // Depth Stencil
    GCustom1,       // Custom Data
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
    static constexpr int RT_COUNT = static_cast<int>(GBufferType::Count);

    // 获取指定 Attachmenet 的配置
    static GBufferRTParam Get(GBufferType type)
    {
        VkImageUsageFlags colorUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        VkImageUsageFlags depthUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        switch (type)
        {
            case GBufferType::GBaseColor:
                return {type, "GBufferA_BaseColor", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GWorldNormal:
                return {type, "GBufferB_WorldNormal", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GPBR:
                return {type, "GBufferC_PBR", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GCustom1:
                return {type, "GBufferD_Custom", VK_FORMAT_R16G16B16A16_SFLOAT, colorUsage};
            case GBufferType::GVelocity:
                // TAA通常只需要 R16G16, 但为了匹配"极致精度"要求，这里也可以选 R32G32 (视需求而定，暂定 R16G16)
                return {type, "GBuffer_Velocity", VK_FORMAT_R16G16_SFLOAT, colorUsage};
            case GBufferType::GSceneDepth:
                return {type, "SceneDepth", VK_FORMAT_D32_SFLOAT_S8_UINT, depthUsage};
            default:
                return {GBufferType::Count, "Invalid", VK_FORMAT_UNDEFINED, 0};
        }
    }
};

} // namespace Play
#endif // GBUFFER_CONFIG_H