#include "Resource.h"
#include "PlayApp.h"
#include "PlayAllocator.h"
namespace Play
{
Texture* Texture::Create()
{
    Texture* res = TexturePool::Instance().alloc();
    return res;
}

Texture* Texture::Create(std::string name)
{
    Texture* res     = TexturePool::Instance().alloc();
    res->DebugName() = name;
    return res;
}
Texture* Texture::Create(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
                         VkImageLayout layout, uint32_t mipLevels, VkSampleCountFlagBits samples)
{
    return TexturePool::Instance().alloc(width, height, format, usage, layout, mipLevels, samples);
}

Texture* Create(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                VkImageUsageFlags usage, VkImageLayout initialLayout, uint32_t mipLevels = 1)
{
    return TexturePool::Instance().alloc(width, height, depth, format, usage, initialLayout,
                                         mipLevels);
}
Texture* Create(uint32_t size, VkFormat format, VkImageUsageFlags usage,
                VkImageLayout initialLayout, uint32_t mipLevels = 1)
{
    return TexturePool::Instance().allocCube(size, format, usage, initialLayout, mipLevels);
}
Texture* Create(const std::filesystem::path& imagePath,
                VkImageLayout                finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                uint32_t                     mipLevels   = 1)
{
    return TexturePool::Instance().alloc(imagePath, finalLayout, mipLevels);
}

void Texture::Destroy(Texture* texture)
{
    TexturePool::Instance().free(texture);
    texture = nullptr;
}

Buffer* Buffer::Create()
{
    return BufferPool::Instance().alloc();
}

Buffer* Buffer::Create(std::string name)
{
    Buffer* res      = BufferPool::Instance().alloc();
    res->DebugName() = name;
    return res;
}

Buffer* Buffer::Create(std::string name, VkBufferUsageFlags2 usage, VkDeviceSize size,
                       VkMemoryPropertyFlags property)
{
    Buffer* res      = BufferPool::Instance().alloc(size, usage, property);
    res->DebugName() = name;
    return res;
}

void Buffer::Destroy(Buffer* buffer)
{
    BufferPool::Instance().free(buffer);
    buffer = nullptr;
}
} // namespace Play