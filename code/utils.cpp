#include "utils.hpp"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"
#include <cstring>
#include "crc32c/crc32c.h"
namespace Play
{

namespace
{
constexpr uint64_t kMul  = 0x9E3779B185EBCA87ull;
constexpr uint64_t kMix1 = 0xFF51AFD7ED558CCDll;
constexpr uint64_t kMix2 = 0xC4CEB9FE1A85EC53ull;

inline uint64_t mix64(uint64_t value)
{
    value ^= value >> 33;
    value *= kMix1;
    value ^= value >> 33;
    value *= kMix2;
    value ^= value >> 33;
    return value;
}
} // namespace

std::string GetUniqueName()
{
    static uint64_t uniqueId = 0;
    return std::to_string(uniqueId++);
}

VkImageCreateInfo makeImage2DCreateInfo(VkExtent2D extent, VkFormat format, VkImageUsageFlags usageFlags, bool mipmap)
{
    VkImageCreateInfo createInfo = {};
    createInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType         = VK_IMAGE_TYPE_2D;
    createInfo.extent            = {extent.width, extent.height, 1};
    createInfo.format            = format;
    createInfo.usage             = usageFlags;
    createInfo.mipLevels         = mipmap ? 0 : 1;
    return createInfo;
}

VkImageCreateInfo makeImage3DCreateInfo(VkExtent3D extent, VkFormat format, VkImageUsageFlags usageFlags, bool mipmap)
{
    VkImageCreateInfo createInfo = {};
    createInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType         = VK_IMAGE_TYPE_3D;
    createInfo.extent            = extent;
    createInfo.format            = format;
    createInfo.usage             = usageFlags;
    createInfo.mipLevels         = mipmap ? 0 : 1;
    return createInfo;
}
uint64_t memoryHash(void* data, size_t size)
{
    return crc32c::Crc32c(reinterpret_cast<char*>(data), size);
}

bool isImageBarrierValid(const VkImageMemoryBarrier2& barrier)
{
    return barrier.srcAccessMask | barrier.dstAccessMask | barrier.oldLayout | barrier.newLayout | barrier.dstStageMask | barrier.srcStageMask;
}
bool isBufferBarrierValid(const VkBufferMemoryBarrier2& barrier)
{
    return barrier.srcAccessMask | barrier.dstAccessMask | barrier.dstStageMask | barrier.srcStageMask | barrier.dstQueueFamilyIndex |
           barrier.srcQueueFamilyIndex;
}
VkImageAspectFlags inferImageAspectFlags(VkFormat format, VkImageUsageFlags usage)
{
    VkImageAspectFlags aspectFlags = 0;

    // 1. 基于 Format 的推导 (这是最准确的)
    switch (format)
    {
        // 深度 + 模板
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            break;

        // 仅深度
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;

        // 仅模板
        case VK_FORMAT_S8_UINT:
            aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
            break;

        // 默认当作颜色
        default:
            aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
            break;
    }

    return aspectFlags;
}

VkAccessFlags2 inferAccessFlags(VkImageLayout layout)
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            return VK_ACCESS_2_NONE;

        case VK_IMAGE_LAYOUT_PREINITIALIZED:
            return VK_ACCESS_2_HOST_WRITE_BIT;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
            return VK_ACCESS_2_SHADER_READ_BIT; // 或者 VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT，视情况而定，通常 Shader Read 更通用

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_2_SHADER_READ_BIT;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_2_TRANSFER_READ_BIT;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_2_TRANSFER_WRITE_BIT;

        case VK_IMAGE_LAYOUT_GENERAL:
            // General 比较特殊，通常用于 Compute Shader 的 Storage Image 读写，或者既读又写
            // 这里做一个比较宽泛的假设，或者你可以传入 Usage 来辅助判断
            return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_ACCESS_2_NONE; // Present 操作通常不需要 Access Mask，或者由 Semaphore 处理

        default:
            return VK_ACCESS_2_NONE;
    }
}
} // namespace Play