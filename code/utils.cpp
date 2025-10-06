#include "utils.hpp"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"
namespace Play
{

std::string GetUniqueName()
{
    static uint64_t uniqueId = 0;
    return std::to_string(uniqueId++);
}

VkAttachmentLoadOp RTState::getVkLoadOp() const
{
    switch (_loadOp)
    {
        case loadOp::eLoad:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case loadOp::eDontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        case loadOp::eClear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        default:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp RTState::getVkStoreOp() const
{
    switch (_storeOp)
    {
        case storeOp::eStore:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case storeOp::eDontCare:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default:
            return VK_ATTACHMENT_STORE_OP_STORE;
    }
}

VkImageCreateInfo makeImage2DCreateInfo(VkExtent2D extent, VkFormat format,
                                        VkImageUsageFlags usageFlags, bool mipmap)
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

VkImageCreateInfo makeImage3DCreateInfo(VkExtent3D extent, VkFormat format,
                                        VkImageUsageFlags usageFlags, bool mipmap)
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
} // namespace Play