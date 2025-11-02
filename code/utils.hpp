#ifndef UTILS_HPP
#define UTILS_HPP
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>
#include "vulkan/vulkan.h"

#define CUSTOM_NAME_VK(DEBUGER, _x)                                          \
    DEBUGER.setObjectName(_x, (std::string(CLASS_NAME) + std::string("::") + \
                               std::string(#_x " (") + NAME_FILE_LOCATION)   \
                                  .c_str())
namespace Play
{
using MAX_RT_NUM          = std::integral_constant<size_t, 16>;
using MAX_DESCRIPTOR_SETS = std::integral_constant<size_t, 5>;
class Texture;
std::string GetUniqueName();

struct RTState
{
    RTState(uint8_t loadop, uint8_t storeop, Texture* texture, Texture* resolveTexture)
        : _loadOp(static_cast<loadOp>(loadop)),
          _storeOp(static_cast<storeOp>(storeop)),
          _texture(texture),
          _resolveTexture(resolveTexture)
    {
    }
    enum class loadOp
    {
        eLoad,
        eDontCare,
        eClear

    } _loadOp;
    enum class storeOp
    {
        eStore,
        eDontCare
    } _storeOp;
    Texture*            _texture;
    Texture*            _resolveTexture;
    VkAttachmentLoadOp  getVkLoadOp() const;
    VkAttachmentStoreOp getVkStoreOp() const;
};

VkImageCreateInfo makeImage2DCreateInfo(VkExtent2D extent, VkFormat format,
                                        VkImageUsageFlags usageFlags, bool mipmap);

VkImageCreateInfo makeImage3DCreateInfo(VkExtent3D extent, VkFormat format,
                                        VkImageUsageFlags usageFlags, bool mipmap);
uint64_t          memoryHash(void* data, size_t size);

template <typename T>
uint64_t memoryHash(const std::vector<T>& data)
{
    return memoryHash((void*) data.data(), data.size() * sizeof(T));
}

} // namespace Play
#endif // UTILS_HPP