#ifndef VULKAN_RESOURCE_UTILS_H
#define VULKAN_RESOURCE_UTILS_H

#include <vulkan/vulkan.h>

namespace Play
{
VkImageCreateInfo makeImage2DCreateInfo(VkExtent2D extent, VkFormat format, VkImageUsageFlags usageFlags, bool mipmap);
VkImageCreateInfo makeImage3DCreateInfo(VkExtent3D extent, VkFormat format, VkImageUsageFlags usageFlags, bool mipmap);

VkImageAspectFlags inferImageAspectFlags(VkFormat format, bool forImageView = false);
VkAccessFlags2     inferAccessFlags(VkImageLayout layout);
} // namespace Play

#endif // VULKAN_RESOURCE_UTILS_H
