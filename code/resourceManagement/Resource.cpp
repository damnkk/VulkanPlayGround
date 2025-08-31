#include "Resource.h"
#include "PlayApp.h"
#include "PlayAllocator.h"
namespace Play
{
Texture* Texture::Create(uint32_t width, uint32_t height, VkFormat format,
                   VkImageUsageFlags usage, VkImageLayout layout,
                   uint32_t mipLevels, VkSampleCountFlagBits samples)
{
    return TexturePool::Instance().alloc(width, height, format, usage, layout, mipLevels, samples);
}

Texture* Create(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
            VkImageUsageFlags usage, VkImageLayout initialLayout,
            uint32_t mipLevels = 1){
    return TexturePool::Instance().alloc(width, height, depth, format, usage, initialLayout, mipLevels);
}
Texture* Create(uint32_t size, VkFormat format,
                VkImageUsageFlags usage, VkImageLayout initialLayout,
                uint32_t mipLevels = 1){
    return TexturePool::Instance().allocCube(size, format, usage, initialLayout, mipLevels);
}
Texture* Create(const std::filesystem::path& imagePath,
                VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                uint32_t mipLevels = 1){
    return TexturePool::Instance().alloc(imagePath, finalLayout, mipLevels);
}

void     Texture::Destroy(Texture* texture){
    TexturePool::Instance().free(texture);
    texture = nullptr;
}
} // namespace Play