#include "Resource.h"
#include "PlayApp.h"
#include "PlayAllocator.h"
#include "VulkanDriver.h"
#include "utils.hpp"
#include "stb_image.h"
#include "nvvk/mipmaps.hpp"

namespace Play
{

// ==================== Texture 实现 ====================

Texture::Texture()
{
    if (vkDriver) vkDriver->registerObject(this);
}

Texture::Texture(std::string name) : Texture()
{
    debugName = std::move(name);
}

Texture::Texture(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, uint32_t mipLevels,
                 VkSampleCountFlagBits samples)
    : Texture()
{
    VkImageCreateInfo imageInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = {width, height, 1},
        .mipLevels     = mipLevels,
        .arrayLayers   = 1,
        .samples       = samples,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = format,
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {inferImageAspectFlags(format, true), 0, mipLevels, 0, 1},
    };
    PlayResourceManager::Instance().createImage(*this, imageInfo, viewInfo);

    if (layout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image            = this->image;
        imageBarrier.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout        = layout;
        imageBarrier.srcAccessMask    = VK_ACCESS_2_NONE;
        imageBarrier.dstAccessMask    = inferAccessFlags(layout);
        imageBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_NONE;
        imageBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.subresourceRange = {inferImageAspectFlags(format, true), 0, mipLevels, 0, 1};
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers    = &imageBarrier;
        auto cmd                     = PlayResourceManager::Instance().getTempCommandBuffer();
        vkCmdPipelineBarrier2(cmd, &info);
        PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);
    }

    descriptor.imageLayout = layout;
    type                   = VK_IMAGE_TYPE_2D;
    this->format           = format;
    extent                 = {width, height, 1};
    sampleCount            = samples;
    usageFlags             = imageInfo.usage;
    aspectFlags =
        (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;
}

Texture::Texture(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageUsageFlags usage, VkImageLayout initialLayout,
                 uint32_t mipLevels)
    : Texture()
{
    VkImageCreateInfo imageInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = depth == 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D,
        .format        = format,
        .extent        = {width, height, depth},
        .mipLevels     = mipLevels,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = depth == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_3D,
        .format           = format,
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {inferImageAspectFlags(format, true), 0, mipLevels, 0, 1},
    };
    PlayResourceManager::Instance().createImage(*this, imageInfo, viewInfo);

    if (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image               = this->image;
        imageBarrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout           = initialLayout;
        imageBarrier.srcAccessMask       = VK_ACCESS_2_NONE;
        imageBarrier.dstAccessMask       = inferAccessFlags(initialLayout);
        imageBarrier.srcStageMask        = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        imageBarrier.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.subresourceRange    = {inferImageAspectFlags(format, usage), 0, mipLevels, 0, 1};
        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers    = &imageBarrier;
        auto cmd                     = vkDriver->_app->createTempCmdBuffer();
        vkCmdPipelineBarrier2(cmd, &info);
        PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);
    }

    descriptor.imageLayout = initialLayout;
    type                   = imageInfo.imageType;
    this->format           = format;
    extent                 = {width, height, depth};
    sampleCount            = VK_SAMPLE_COUNT_1_BIT;
    usageFlags             = imageInfo.usage;
    aspectFlags            = (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
}

Texture::Texture(uint32_t size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initialLayout, uint32_t mipLevels) : Texture()
{
    VkImageCreateInfo imageInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = {size, size, 1},
        .mipLevels     = mipLevels,
        .arrayLayers   = 6,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType         = VK_IMAGE_VIEW_TYPE_CUBE,
        .format           = format,
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 6},
    };
    PlayResourceManager::Instance().createImage(*this, imageInfo, viewInfo);

    if (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image            = this->image;
        imageBarrier.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout        = initialLayout;
        imageBarrier.srcAccessMask    = VK_ACCESS_2_NONE;
        imageBarrier.dstAccessMask    = inferAccessFlags(initialLayout);
        imageBarrier.subresourceRange = {inferImageAspectFlags(format, usage), 0, mipLevels, 0, 6};
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers    = &imageBarrier;
        auto cmd                     = vkDriver->_app->createTempCmdBuffer();
        vkCmdPipelineBarrier2(cmd, &info);
        PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);
    }

    descriptor.imageLayout = initialLayout;
    type                   = VK_IMAGE_TYPE_2D;
    this->format           = format;
    extent                 = {size, size, 1};
    sampleCount            = VK_SAMPLE_COUNT_1_BIT;
    usageFlags             = imageInfo.usage;
    aspectFlags =
        (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;
}

Texture::Texture(const std::filesystem::path& imagePath, VkImageLayout finalLayout, uint32_t mipLevels, bool isSrgb) : Texture()
{
    debugName = nvutils::utf8FromPath(imagePath);

    // HDR 图片处理
    if (stbi_is_hdr(nvutils::utf8FromPath(imagePath).c_str()))
    {
        int    width, height, channels;
        float* data = stbi_loadf(nvutils::utf8FromPath(imagePath).c_str(), &width, &height, &channels, 4);
        if (!data)
        {
            LOGW("Failed to load hdr image: %s\n", nvutils::utf8FromPath(imagePath).c_str());
            return;
        }

        // 创建 image
        VkImageCreateInfo imageInfo{
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_2D,
            .format        = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent        = {(uint32_t) width, (uint32_t) height, 1},
            .mipLevels     = mipLevels,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VkImageViewCreateInfo viewInfo{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
            .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1},
        };
        PlayResourceManager::Instance().createImage(*this, imageInfo, viewInfo);

        // 上传数据
        VkDeviceSize dataSize = static_cast<VkDeviceSize>(width) * height * sizeof(float) * 4;
        auto         cmd      = PlayResourceManager::Instance().getTempCommandBuffer();
        PlayResourceManager::Instance().appendImage(*this, dataSize, data, finalLayout);
        PlayResourceManager::Instance().cmdUploadAppended(cmd);
        nvvk::cmdGenerateMipmaps(cmd, image, {(uint32_t) width, (uint32_t) height}, mipLevels, 1, finalLayout);
        PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);
        PlayResourceManager::Instance().acquireSampler(descriptor.sampler);

        // 设置成员变量
        descriptor.imageLayout = finalLayout;
        type                   = VK_IMAGE_TYPE_2D;
        format                 = VK_FORMAT_R32G32B32A32_SFLOAT;
        extent                 = {(uint32_t) width, (uint32_t) height, 1};
        sampleCount            = VK_SAMPLE_COUNT_1_BIT;
        usageFlags             = imageInfo.usage;
        aspectFlags            = VK_IMAGE_ASPECT_COLOR_BIT;

        stbi_image_free(data);
        return;
    }

    // 普通图片处理
    const std::string imageFileContents = nvutils::loadFile(imagePath);
    if (imageFileContents.empty())
    {
        LOGW("File was empty or could not be opened: %s\n", nvutils::utf8FromPath(imagePath).c_str());
        return;
    }

    const stbi_uc* imageFileData = reinterpret_cast<const stbi_uc*>(imageFileContents.data());
    if (imageFileContents.size() > std::numeric_limits<int>::max())
    {
        LOGW("File too large for stb_image to read: %s\n", nvutils::utf8FromPath(imagePath).c_str());
        return;
    }
    const int imageFileSize = static_cast<int>(imageFileContents.size());

    int w = 0, h = 0, comp = 0;
    if (!stbi_info_from_memory(imageFileData, imageFileSize, &w, &h, &comp))
    {
        LOGW("Failed to get info for %s\n", nvutils::utf8FromPath(imagePath).c_str());
        return;
    }

    const bool is16Bit = stbi_is_16_bit_from_memory(imageFileData, imageFileSize);
    stbi_uc*   data    = nullptr;
    size_t     bytes_per_pixel;
    int        requiredComponents = comp == 1 ? 1 : 4;

    if (is16Bit)
    {
        stbi_us* data16 = stbi_load_16_from_memory(imageFileData, imageFileSize, &w, &h, &comp, requiredComponents);
        bytes_per_pixel = sizeof(*data16) * requiredComponents;
        data            = reinterpret_cast<stbi_uc*>(data16);
    }
    else
    {
        data            = stbi_load_from_memory(imageFileData, imageFileSize, &w, &h, &comp, requiredComponents);
        bytes_per_pixel = sizeof(*data) * requiredComponents;
    }

    if (data && w > 0 && h > 0)
    {
        VkFormat format = VK_FORMAT_UNDEFINED;
        switch (requiredComponents)
        {
            case 1:
                format = is16Bit ? VK_FORMAT_R16_UNORM : VK_FORMAT_R8_UNORM;
                break;
            case 4:
                format = is16Bit ? VK_FORMAT_R16G16B16A16_UNORM : isSrgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
                break;
        }

        // 创建 image
        VkImageCreateInfo imageInfo{
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_2D,
            .format        = format,
            .extent        = {(uint32_t) w, (uint32_t) h, 1},
            .mipLevels     = mipLevels,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VkImageViewCreateInfo viewInfo{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = format,
            .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A},
            .subresourceRange = {inferImageAspectFlags(format, true), 0, mipLevels, 0, 1},
        };
        PlayResourceManager::Instance().createImage(*this, imageInfo, viewInfo);

        // 上传数据
        VkDeviceSize dataSize = static_cast<VkDeviceSize>(w) * h * bytes_per_pixel;
        auto         cmd      = PlayResourceManager::Instance().getTempCommandBuffer();
        PlayResourceManager::Instance().appendImage(*this, dataSize, data, finalLayout);
        PlayResourceManager::Instance().cmdUploadAppended(cmd);
        nvvk::cmdGenerateMipmaps(cmd, image, {(uint32_t) w, (uint32_t) h}, mipLevels, 1, finalLayout);
        PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);
        PlayResourceManager::Instance().acquireSampler(descriptor.sampler);

        // 设置成员变量
        descriptor.imageLayout = finalLayout;
        type                   = VK_IMAGE_TYPE_2D;
        this->format           = format;
        extent                 = {(uint32_t) w, (uint32_t) h, 1};
        sampleCount            = VK_SAMPLE_COUNT_1_BIT;
        usageFlags             = imageInfo.usage;
        aspectFlags            = VK_IMAGE_ASPECT_COLOR_BIT;

        stbi_image_free(data);
    }
    else
    {
        stbi_image_free(data);
    }
}

void Texture::onDestroy()
{
    if (vkDriver) vkDriver->unregisterObject(this);

    if (image != VK_NULL_HANDLE)
    {
        nvvk::Image capturedImage;
        capturedImage.image       = this->image;
        capturedImage.allocation  = this->allocation;
        capturedImage.descriptor  = this->descriptor;
        capturedImage.extent      = this->extent;
        capturedImage.mipLevels   = this->mipLevels;
        capturedImage.arrayLayers = this->arrayLayers;
        capturedImage.format      = this->format;

        vkDriver->deferDestroy([capturedImage]() mutable { PlayResourceManager::Instance().destroyImage(capturedImage); });

        image                = VK_NULL_HANDLE;
        descriptor.imageView = VK_NULL_HANDLE;
    }
}

// ==================== Buffer 实现 ====================

Buffer::Buffer()
{
    if (vkDriver) vkDriver->registerObject(this);
}

Buffer::Buffer(std::string name) : Buffer()
{
    debugName = std::move(name);
}

Buffer::Buffer(std::string name, VkBufferUsageFlags2 usage, VkDeviceSize bufSize, VkMemoryPropertyFlags property) : Buffer()
{
    debugName = std::move(name);

    VmaMemoryUsage           vmaUsage;
    VmaAllocationCreateFlags flags = 0;
    if (property & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        vmaUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    }
    else if (property & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        vmaUsage = (property & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_CPU_ONLY;
        flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    else
    {
        vmaUsage = VMA_MEMORY_USAGE_AUTO;
    }

    PlayResourceManager::Instance().createBuffer(*this, bufSize, usage, vmaUsage, flags);
    this->bufferSize = bufSize;
    this->usageFlags = usage;
    this->property   = property;
}

void Buffer::onDestroy()
{
    if (vkDriver) vkDriver->unregisterObject(this);

    if (buffer != VK_NULL_HANDLE)
    {
        nvvk::Buffer capturedBuffer;
        capturedBuffer.buffer     = this->buffer;
        capturedBuffer.allocation = this->allocation;
        capturedBuffer.bufferSize = this->bufferSize;
        capturedBuffer.address    = this->address;
        capturedBuffer.mapping    = this->mapping;

        vkDriver->deferDestroy([capturedBuffer]() mutable { PlayResourceManager::Instance().destroyBuffer(capturedBuffer); });

        buffer = VK_NULL_HANDLE;
    }
}

} // namespace Play
