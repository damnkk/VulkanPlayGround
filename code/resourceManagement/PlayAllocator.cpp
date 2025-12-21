#include "PlayAllocator.h"
#include "Resource.h"
#include "VulkanDriver.h"
#include "utils.hpp"
#include "nvvk/check_error.hpp"
#include "nvvk/debug_util.hpp"
#include "nvvk/mipmaps.hpp"
#include "stb_image.h"
namespace Play
{
TexturePool& TexturePool::Instance()
{
    static TexturePool pool;
    return pool;
}

Texture* TexturePool::alloc()
{
    assert(_availableIndex < _objs.size());
    uint32_t index = _freeIndices[_availableIndex++];
    if (_objs[index] != nullptr)
    {
        _objs[index]->poolId = index;
        return _objs[index];
    }
    else
    {
        _objs[index] = static_cast<Texture*>(new Texture(index));
    }
    return _objs[index];
}

void TexturePool::free(Texture* obj)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (obj == nullptr || !(obj->poolId < _objs.size() && obj->poolId >= 0))
    {
        return;
    }
    _freeIndices[--_availableIndex] = obj->poolId;
    _manager->destroyImage(*obj);
    obj->poolId = -1;
};

Texture* TexturePool::alloc(VkImageCreateInfo* imgInfo, VkImageViewCreateInfo* viewInfo)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Texture*                    texture = alloc();
    if (!texture)
    {
        LOGE("Failed to allocate texture");
        return nullptr;
    }
    NVVK_CHECK(_manager->createImage(*texture, *imgInfo, *viewInfo));
    return texture;
}

Texture* TexturePool::alloc(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, uint32_t mipLevels,
                            VkSampleCountFlagBits samples)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Texture*                    texture = alloc();
    if (!texture)
    {
        LOGE("Failed to allocate texture");
        return nullptr;
    }
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
        .subresourceRange = {inferImageAspectFlags(format, usage), 0, mipLevels, 0, 1},
    };
    NVVK_CHECK(_manager->createImage(*texture, imageInfo, viewInfo));

    if (layout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image            = texture->image;
        imageBarrier.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout        = layout;
        imageBarrier.srcAccessMask    = VK_ACCESS_2_NONE;
        imageBarrier.dstAccessMask    = inferAccessFlags(layout);
        imageBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        imageBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.subresourceRange = {inferImageAspectFlags(format, usage), 0, mipLevels, 0, 1};
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers    = &imageBarrier;
        auto cmd                     = _manager->getTempCommandBuffer();
        vkCmdPipelineBarrier2(cmd, &info);
        _manager->submitAndWaitTempCmdBuffer(cmd);
    }

    texture->descriptor.imageLayout = layout;
    texture->type                   = VK_IMAGE_TYPE_2D;
    texture->format                 = format;
    texture->extent                 = {width, height, 1};
    texture->sampleCount            = samples;
    texture->usageFlags             = imageInfo.usage;
    texture->aspectFlags =
        (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;
    return texture;
}

Texture* TexturePool::alloc(uint32_t width, uint32_t height, uint32_t depth, VkFormat format, VkImageUsageFlags usage, VkImageLayout initialLayout,
                            uint32_t mipLevels)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Texture*                    texture = alloc();
    if (!texture)
    {
        return nullptr;
    }
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
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1},
    };
    if (_manager->createImage(*texture, imageInfo, viewInfo) != VK_SUCCESS)
    {
        free(texture);
        return nullptr;
    }
    if (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image            = texture->image;
        imageBarrier.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout        = initialLayout;
        imageBarrier.srcAccessMask    = VK_ACCESS_2_NONE;
        imageBarrier.dstAccessMask    = inferAccessFlags(initialLayout);
        imageBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        imageBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.subresourceRange = {inferImageAspectFlags(format, usage), 0, mipLevels, 0, 1};
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers    = &imageBarrier;
        auto cmd                     = vkDriver->_app->createTempCmdBuffer();
        vkCmdPipelineBarrier2(cmd, &info);
        _manager->submitAndWaitTempCmdBuffer(cmd);
    }
    texture->descriptor.imageLayout = initialLayout;
    texture->type                   = imageInfo.imageType;
    texture->format                 = format;
    texture->extent                 = {width, height, depth};
    texture->sampleCount            = VK_SAMPLE_COUNT_1_BIT;
    texture->usageFlags             = imageInfo.usage;
    texture->aspectFlags =
        (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;
    return texture;
}

Texture* TexturePool::allocCube(uint32_t size, VkFormat format, VkImageUsageFlags usage, VkImageLayout initialLayout, uint32_t mipLevels)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Texture*                    texture = alloc();
    if (!texture)
    {
        return nullptr;
    }
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
    if (_manager->createImage(*texture, imageInfo, viewInfo) != VK_SUCCESS)
    {
        free(texture);
        return nullptr;
    }
    if (initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image            = texture->image;
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
        _manager->submitAndWaitTempCmdBuffer(cmd);
    }

    texture->descriptor.imageLayout = initialLayout;
    texture->type                   = VK_IMAGE_TYPE_2D;
    texture->format                 = format;
    texture->extent                 = {size, size, 1};
    texture->sampleCount            = VK_SAMPLE_COUNT_1_BIT;
    texture->usageFlags             = imageInfo.usage;
    texture->aspectFlags =
        (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;
    return texture;
}

Texture* TexturePool::alloc(const void* data, size_t dataSize, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
                            uint32_t mipLevels, VkImageLayout layout)
{
    Texture* texture = alloc(width, height, format, usage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
    auto     cmd     = _manager->getTempCommandBuffer();
    _manager->appendImage(*texture, dataSize, data, layout);
    _manager->cmdUploadAppended(cmd);
    nvvk::cmdGenerateMipmaps(cmd, texture->image, {width, height}, mipLevels, 1, layout);
    _manager->submitAndWaitTempCmdBuffer(cmd);
    _manager->acquireSampler(texture->descriptor.sampler);
    return texture;
}

Texture* TexturePool::alloc(const std::filesystem::path& imagePath, VkImageLayout finalLayout, uint32_t mipLevels, bool isSrgb)
{
    const std::string imageFileContents = nvutils::loadFile(imagePath);
    if (imageFileContents.empty())
    {
        LOGW("File was empty or could not be opened: %s\n", nvutils::utf8FromPath(imagePath).c_str());
        return nullptr;
    }
    const stbi_uc* imageFileData = reinterpret_cast<const stbi_uc*>(imageFileContents.data());
    if (imageFileContents.size() > std::numeric_limits<int>::max())
    {
        LOGW("File too large for stb_image to read: %s\n", nvutils::utf8FromPath(imagePath).c_str());
        return nullptr;
    }
    const int imageFileSize = static_cast<int>(imageFileContents.size());

    // Read the header once to check how many channels it has. We can't trivially use
    // RGB/VK_FORMAT_R8G8B8_UNORM and need to set requiredComponents=4 in such cases.
    int w = 0, h = 0, comp = 0;
    if (!stbi_info_from_memory(imageFileData, imageFileSize, &w, &h, &comp))
    {
        LOGW("Failed to get info for %s\n", nvutils::utf8FromPath(imagePath).c_str());
        return nullptr;
    }

    // Read the header again to check if it has 16 bit data, e.g. for a heightmap.
    const bool is16Bit = stbi_is_16_bit_from_memory(imageFileData, imageFileSize);

    // Load the image
    stbi_uc* data = nullptr;
    size_t   bytes_per_pixel;
    int      requiredComponents = comp == 1 ? 1 : 4;
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

    // Make a copy of the image data to be uploaded to vulkan later
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

        VkDeviceSize buffer_size = static_cast<VkDeviceSize>(w) * h * bytes_per_pixel;
        Texture*     texture =
            alloc(data, buffer_size, (uint32_t) w, (uint32_t) h, format,
                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, mipLevels, finalLayout);

        texture->DebugName() = nvutils::utf8FromPath(imagePath);
        stbi_image_free(data);
        return texture;
    }
    stbi_image_free(data);
    return nullptr;
}

void TexturePool::deinit()
{
    for (auto& obj : _objs)
    {
        if (obj && obj->poolId >= 0)
        {
            _manager->destroyImage(*obj);
            obj->poolId = -1;
            delete (obj);
        }
    }
}

BufferPool& BufferPool::Instance()
{
    static BufferPool pool;
    return pool;
}

Buffer* BufferPool::alloc()
{
    assert(_availableIndex < _objs.size());
    uint32_t index = _freeIndices[_availableIndex++];
    _objs[index]   = static_cast<Buffer*>(new Buffer(index));
    return _objs[index];
}

Buffer* BufferPool::alloc(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Buffer*                     buffer = alloc();
    if (!buffer)
    {
        LOGE("Failed to allocate buffer");
        return nullptr;
    }
    VmaMemoryUsage vmaUsage;
    if (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    {
        vmaUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    }
    else if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        vmaUsage = (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? VMA_MEMORY_USAGE_CPU_TO_GPU : VMA_MEMORY_USAGE_CPU_ONLY;
    }
    else
    {
        vmaUsage = VMA_MEMORY_USAGE_AUTO;
    }

    _manager->createBuffer(*buffer, size, usage, vmaUsage);
    buffer->size       = size;
    buffer->usageFlags = usage;
    buffer->property   = properties;

    return buffer;
}

Buffer* BufferPool::alloc(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Buffer*                     buffer = alloc(size, usage, properties);
    if (!buffer) return nullptr;

    _manager->appendBuffer(*buffer, 0, size, data);
    auto cmd = _manager->getTempCommandBuffer();
    _manager->cmdUploadAppended(cmd);
    _manager->submitAndWaitTempCmdBuffer(cmd);
    return buffer;
}

Buffer* BufferPool::alloc(VkBufferCreateInfo& bufferInfo)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return alloc(bufferInfo.size, (VkBufferUsageFlags) (bufferInfo.usage), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void BufferPool::free(Buffer* obj)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (obj == nullptr || !(obj->poolId < _objs.size() && obj->poolId >= 0))
    {
        return;
    }
    _freeIndices[--_availableIndex] = obj->poolId;
    _manager->destroyBuffer(*obj);
    obj->poolId = -1;
};

void BufferPool::deinit()
{
    for (auto& obj : _objs)
    {
        if (obj && obj->poolId >= 0)
        {
            _manager->destroyBuffer(*obj);
            obj->poolId = -1;
            delete (obj);
        }
    }
}

PlayResourceManager& PlayResourceManager::Instance()
{
    static PlayResourceManager manager;
    return manager;
}

void PlayResourceManager::initialize()
{
    VmaAllocatorCreateInfo allocatorInfo = {
        .flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = vkDriver->_physicalDevice,
        .device         = vkDriver->_device,
        .instance       = vkDriver->_instance,
    };
    ::nvvk::ResourceAllocator::init(allocatorInfo);
    ::nvvk::StagingUploader::init(this);
    ::nvvk::SamplerPool::init(allocatorInfo.device);
    VkCommandPoolCreateInfo cmdPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cmdPoolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdPoolInfo.queueFamilyIndex = vkDriver->_app->getQueue(2).familyIndex;
    vkCreateCommandPool(vkDriver->_device, &cmdPoolInfo, nullptr, &_tempCmdPool);
    this->m_enableLayoutBarriers = true;
}
void PlayResourceManager::deInit()
{
    ::nvvk::StagingUploader::deinit();
    ::nvvk::SamplerPool::deinit();
    ::nvvk::ResourceAllocator::deinit();
    vkDestroyCommandPool(vkDriver->_device, _tempCmdPool, nullptr);
}

VkCommandBuffer PlayResourceManager::getTempCommandBuffer()
{
    return vkDriver->_app->createTempCmdBuffer();
}
void PlayResourceManager::submitAndWaitTempCmdBuffer(VkCommandBuffer cmd)
{
    vkDriver->_app->submitAndWaitTempCmdBuffer(cmd);
}

nvvk::ResourceAllocatorExport* PlayResourceManager::GetAsAllocator()
{
    return static_cast<nvvk::ResourceAllocatorExport*>(&Instance());
}
nvvk::StagingUploader* PlayResourceManager::GetAsStagingUploader()
{
    return static_cast<nvvk::StagingUploader*>(&Instance());
}
nvvk::SamplerPool* PlayResourceManager::GetAsSamplerPool()
{
    return static_cast<nvvk::SamplerPool*>(&Instance());
}

} // namespace Play