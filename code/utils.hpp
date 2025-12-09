#ifndef UTILS_HPP
#define UTILS_HPP
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <vulkan/vulkan.h>

#define CUSTOM_NAME_VK(DEBUGER, _x) \
    DEBUGER.setObjectName(_x, (std::string(CLASS_NAME) + std::string("::") + std::string(#_x " (") + NAME_FILE_LOCATION).c_str())
namespace Play
{
std::string           GetUniqueName();
std::filesystem::path getBaseFilePath();
// memory hash function
uint64_t memoryHash(void* data, size_t size);

template <typename T>
uint64_t memoryHash(const std::vector<T>& data)
{
    return memoryHash((void*) data.data(), data.size() * sizeof(T));
}

//
bool isImageBarrierValid(const VkImageMemoryBarrier2& barrier);
bool isBufferBarrierValid(const VkBufferMemoryBarrier2& barrier);

VkImageAspectFlags inferImageAspectFlags(VkFormat format, VkImageUsageFlags usage = 0);
VkAccessFlags2     inferAccessFlags(VkImageLayout layout);
} // namespace Play
#endif // UTILS_HPP