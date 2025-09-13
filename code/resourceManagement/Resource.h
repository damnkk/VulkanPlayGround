#ifndef RESOURCE_H
#define RESOURCE_H
#include "stdint.h"
#include "vector"
#include "nvvk/resources.hpp"
#include "PlayAllocator.h"
namespace Play
{
class PlayAllocator;
class Texture;
class Buffer;

class Texture : public nvvk::Image
{
   public:
    static Texture* Create(uint32_t width, uint32_t height, VkFormat format,
                           VkImageUsageFlags usage,
                           VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED, uint32_t mipLevels = 1,
                           VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    static Texture* Create(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                           VkImageUsageFlags usage, VkImageLayout initialLayout,
                           uint32_t mipLevels = 1);
    static Texture* Create(uint32_t size, VkFormat format, VkImageUsageFlags usage,
                           VkImageLayout initialLayout, uint32_t mipLevels = 1);
    static Texture* Create(const std::filesystem::path& imagePath,
                           VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           uint32_t      mipLevels   = 1);
    static void     Destroy(Texture* texture);
    struct TexMetaData
    {
        VkFormat              format = VK_FORMAT_UNDEFINED;
        VkImageType           type   = VK_IMAGE_TYPE_2D;
        VkExtent3D            extent;
        VkImageUsageFlags     usageFlags;
        VkImageAspectFlags    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        uint32_t              mipmapLevel = 1;
        uint32_t              layerCount  = 1;
        std::string           debugName;
    };
    Texture(int poolID) : poolId(poolID) {};
    int Id()
    {
        return poolId;
    }
    VkFormat& Format()
    {
        return format;
    }
    VkImageType& Type()
    {
        return type;
    }
    VkExtent3D& Extent()
    {
        return extent;
    }
    VkSampleCountFlagBits& SampleCount()
    {
        return sampleCount;
    }
    std::string& DebugName()
    {
        return debugName;
    }
    uint32_t& MipLevel()
    {
        return mipLevels;
    }
    bool isDepth() const
    {
        return aspectFlags & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    }
    VkImageLayout& Layout()
    {
        return descriptor.imageLayout;
    }
    VkImageUsageFlags& UsageFlags()
    {
        return usageFlags;
    }
    VkImageAspectFlags& AspectFlags()
    {
        return aspectFlags;
    }
    uint32_t& LayerCount()
    {
        return arrayLayers;
    }
    TexMetaData MetaData()
    {
        return {format,      type,      extent,      usageFlags, aspectFlags,
                sampleCount, mipLevels, arrayLayers, debugName};
    }

    void setMetaData(TexMetaData& metadata)
    {
        format      = metadata.format;
        type        = metadata.type;
        extent      = metadata.extent;
        usageFlags  = metadata.usageFlags;
        aspectFlags = metadata.aspectFlags;
        sampleCount = metadata.sampleCount;
        mipLevels   = metadata.mipmapLevel;
        arrayLayers = metadata.layerCount;
        debugName   = metadata.debugName;
    }

   protected:
    friend class TexturePool;
    int                   poolId      = -1;
    VkImageType           type        = VK_IMAGE_TYPE_2D;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    std::string           debugName;
    VkImageAspectFlags    aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageUsageFlags     usageFlags =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
};

class Buffer : public nvvk::Buffer
{
   public:
    static Buffer* Create();
    static void    Destroy(Buffer* buffer);

    struct BufferMetaData
    {
        VkBufferUsageFlags    _usageFlags;
        VkDeviceSize          _size;
        VkDeviceSize          _range = VK_WHOLE_SIZE;
        VkMemoryPropertyFlags _property;
    };
    Buffer(int poolID) : poolId(poolID) {};
    int Id()
    {
        return poolId;
    }
    int                    poolId = -1;
    VkDescriptorBufferInfo descriptor;
    std::string&           DebugName()
    {
        return debugName;
    }
    VkBufferUsageFlags& UsageFlags()
    {
        return usageFlags;
    }
    VkDeviceSize& BufferSize()
    {
        return bufferSize;
    }
    VkDeviceSize& BufferRange()
    {
        return range;
    }
    VkMemoryPropertyFlags& BufferProperty()
    {
        return property;
    }

    BufferMetaData getMetaData()
    {
        return BufferMetaData{usageFlags, bufferSize, range, property};
    }

    void setMetaData(BufferMetaData& metadata)
    {
        usageFlags = metadata._usageFlags;
        size       = metadata._size;
        range      = metadata._range;
        property   = metadata._property;
    }

   protected:
    friend class BufferPool;
    VkBufferUsageFlags    usageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkDeviceSize          range      = VK_WHOLE_SIZE;
    VkDeviceSize          size       = 0;
    std::string           debugName;
    VkMemoryPropertyFlags property;
};
} // namespace Play
#endif // RESOURCE_H