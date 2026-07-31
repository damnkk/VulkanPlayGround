#include "VulkanResourceUtils.h"

namespace Play
{
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

VkImageAspectFlags inferImageAspectFlags(VkFormat format, bool forImageView)
{
    VkImageAspectFlags aspectFlags = 0;

    switch (format)
    {
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            if (forImageView)
            {
                aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            }
            else
            {
                aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            break;

        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;

        case VK_FORMAT_S8_UINT:
            aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
            break;

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
            return VK_ACCESS_2_SHADER_READ_BIT;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_2_SHADER_READ_BIT;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_2_TRANSFER_READ_BIT;

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_2_TRANSFER_WRITE_BIT;

        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_ACCESS_2_NONE;

        default:
            return VK_ACCESS_2_NONE;
    }
}

VkFormat toVkFormat(const vpgloader::TextureFormat& format, bool isSrgb)
{
    switch (format.componentType)
    {
        case vpgloader::TextureComponentType::UInt8:
            switch (format.channels)
            {
                case 1:
                    return isSrgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
                case 2:
                    return isSrgb ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
                case 3:
                    return isSrgb ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
                case 4:
                    return isSrgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            }
            break;
        case vpgloader::TextureComponentType::UInt16:
            switch (format.channels)
            {
                case 1:
                    return VK_FORMAT_R16_UNORM;
                case 2:
                    return VK_FORMAT_R16G16_UNORM;
                case 3:
                    return VK_FORMAT_R16G16B16_UNORM;
                case 4:
                    return VK_FORMAT_R16G16B16A16_UNORM;
            }
            break;
        case vpgloader::TextureComponentType::Float16:
            switch (format.channels)
            {
                case 1:
                    return VK_FORMAT_R16_SFLOAT;
                case 2:
                    return VK_FORMAT_R16G16_SFLOAT;
                case 3:
                    return VK_FORMAT_R16G16B16_SFLOAT;
                case 4:
                    return VK_FORMAT_R16G16B16A16_SFLOAT;
            }
            break;
        case vpgloader::TextureComponentType::Float32:
            switch (format.channels)
            {
                case 1:
                    return VK_FORMAT_R32_SFLOAT;
                case 2:
                    return VK_FORMAT_R32G32_SFLOAT;
                case 3:
                    return VK_FORMAT_R32G32B32_SFLOAT;
                case 4:
                    return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            break;
    }
    return VK_FORMAT_UNDEFINED;
}
} // namespace Play
