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
} // namespace Play