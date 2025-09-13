#include "PlayAllocator.h"
#include "Resource.h"
#include "PlayApp.h"
#include "nvvk/check_error.hpp"
#include "nvvk/debug_util.hpp"
#include "nvvk/mipmaps.hpp"
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

Texture* TexturePool::alloc(uint32_t width, uint32_t height, VkFormat format,
                            VkImageUsageFlags usage, VkImageLayout layout, uint32_t mipLevels,
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
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                             VK_COMPONENT_SWIZZLE_A},
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1},
    };
    NVVK_CHECK(_manager->createImage(*texture, imageInfo, viewInfo));

    if (layout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        VkImageMemoryBarrier2 imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.image            = texture->image;
        imageBarrier.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout        = layout;
        imageBarrier.srcAccessMask    = VK_ACCESS_2_NONE;
        imageBarrier.dstAccessMask    = VK_ACCESS_2_NONE;
        imageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1};
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers    = &imageBarrier;
        auto cmd                     = _manager->_element->getApp()->createTempCmdBuffer();
        vkCmdPipelineBarrier2(cmd, &info);
        _manager->submitAndWaitTempCmdBuffer(cmd);
    }

    texture->descriptor.imageLayout = layout;
    texture->type                   = VK_IMAGE_TYPE_2D;
    texture->format                 = format;
    texture->extent                 = {width, height, 1};
    texture->sampleCount            = samples;
    texture->usageFlags             = imageInfo.usage;
    texture->aspectFlags            = (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                                          ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                                          : VK_IMAGE_ASPECT_COLOR_BIT;
    return texture;
}

Texture* TexturePool::alloc(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                            VkImageUsageFlags usage, VkImageLayout initialLayout,
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
        .imageType     = VK_IMAGE_TYPE_3D,
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
        .viewType         = VK_IMAGE_VIEW_TYPE_3D,
        .format           = format,
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                             VK_COMPONENT_SWIZZLE_A},
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
        imageBarrier.image     = texture->image;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = initialLayout;
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers    = &imageBarrier;
        auto cmd                     = _manager->_element->getApp()->createTempCmdBuffer();
        vkCmdPipelineBarrier2(cmd, &info);
        _manager->submitAndWaitTempCmdBuffer(cmd);
    }
    texture->descriptor.imageLayout = initialLayout;
    texture->type                   = VK_IMAGE_TYPE_3D;
    texture->format                 = format;
    texture->extent                 = {width, height, depth};
    texture->sampleCount            = VK_SAMPLE_COUNT_1_BIT;
    texture->usageFlags             = imageInfo.usage;
    texture->aspectFlags            = (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                                          ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                                          : VK_IMAGE_ASPECT_COLOR_BIT;
    return texture;
}

Texture* TexturePool::allocCube(uint32_t size, VkFormat format, VkImageUsageFlags usage,
                                VkImageLayout initialLayout, uint32_t mipLevels)
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
        .components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                             VK_COMPONENT_SWIZZLE_A},
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
        imageBarrier.image     = texture->image;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = initialLayout;
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.imageMemoryBarrierCount = 1;
        info.pImageMemoryBarriers    = &imageBarrier;
        auto cmd                     = _manager->_element->getApp()->createTempCmdBuffer();
        vkCmdPipelineBarrier2(cmd, &info);
        _manager->submitAndWaitTempCmdBuffer(cmd);
    }

    texture->descriptor.imageLayout = initialLayout;
    texture->type                   = VK_IMAGE_TYPE_2D;
    texture->format                 = format;
    texture->extent                 = {size, size, 1};
    texture->sampleCount            = VK_SAMPLE_COUNT_1_BIT;
    texture->usageFlags             = imageInfo.usage;
    texture->aspectFlags            = (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                                          ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                                          : VK_IMAGE_ASPECT_COLOR_BIT;
    return texture;
}

Texture* TexturePool::alloc(const void* data, size_t dataSize, uint32_t width, uint32_t height,
                            VkFormat format, VkImageUsageFlags usage, uint32_t mipLevels)
{
    std::lock_guard<std::mutex> lock(_mutex);
    Texture* texture = alloc(width, height, format, usage, VK_IMAGE_LAYOUT_UNDEFINED, mipLevels);
    auto     cmd     = _manager->getTempCommandBuffer();
    _manager->appendImage(*texture, dataSize, data, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _manager->cmdUploadAppended(cmd);
    nvvk::cmdGenerateMipmaps(cmd, texture->image, {width, height}, mipLevels);
    _manager->submitAndWaitTempCmdBuffer(cmd);
    _manager->acquireSampler(texture->descriptor.sampler);
    return texture;
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

Buffer* BufferPool::alloc(VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties)
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
        vmaUsage = (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? VMA_MEMORY_USAGE_CPU_TO_GPU
                                                                       : VMA_MEMORY_USAGE_CPU_ONLY;
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

Buffer* BufferPool::alloc(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties)
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
    return alloc(bufferInfo.size, (VkBufferUsageFlags) (bufferInfo.usage),
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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

void PlayResourceManager::initialize(PlayElement* element)
{
    _element                             = element;
    VmaAllocatorCreateInfo allocatorInfo = {
        .flags          = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = element->getApp()->getPhysicalDevice(),
        .device         = element->getApp()->getDevice(),
        .instance       = element->getApp()->getInstance(),
    };
    ::nvvk::ResourceAllocator::init(allocatorInfo);
    ::nvvk::StagingUploader::init(this);
    ::nvvk::SamplerPool::init(allocatorInfo.device);
}
void PlayResourceManager::deInit()
{
    ::nvvk::ResourceAllocator::deinit();
    ::nvvk::StagingUploader::deinit();
    ::nvvk::SamplerPool::deinit();
}

VkCommandBuffer PlayResourceManager::getTempCommandBuffer()
{
    if (!_element)
    {
        LOGE("PlayResourceManager not initialized!");
        return VK_NULL_HANDLE;
    }
    return _element->getApp()->createTempCmdBuffer();
}
void PlayResourceManager::submitAndWaitTempCmdBuffer(VkCommandBuffer cmd)
{
    if (!_element)
    {
        LOGE("PlayResourceManager not initialized!");
        return;
    }
    _element->getApp()->submitAndWaitTempCmdBuffer(cmd);
}

} // namespace Play