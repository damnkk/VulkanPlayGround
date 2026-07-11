#ifndef IMAGE_LOADING_H
#define IMAGE_LOADING_H

#include <cstdint>
#include <filesystem>
#include <vector>
#include <vulkan/vulkan.h>

namespace Play::ImageLoading
{
struct LoadedImage
{
    VkFormat             format = VK_FORMAT_UNDEFINED;
    VkExtent2D           extent = {};
    std::vector<uint8_t> pixels;
};

bool LoadTextureImage(const std::filesystem::path& imagePath, bool isSrgb, LoadedImage& loadedImage);
} // namespace Play::ImageLoading

#endif // IMAGE_LOADING_H
