#ifndef RESOURCE_H
#define RESOURCE_H
#include "stdint.h"
#include "vector"
#include "nvvk/memallocator_vma_vk.hpp"
#include "host_device.h"
namespace Play
{
struct Texture : public nvvk::Texture
{
    Texture() = default;
    Texture(int poolID) : _poolId(poolID) {};
    VkFormat    _format      = VK_FORMAT_UNDEFINED;
    VkImageType _type        = VK_IMAGE_TYPE_2D;
    VkExtent3D _extent;
    VkImageUsageFlags _usageFlags;
    VkImageAspectFlags _aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSampleCountFlags _sampleCount = VK_SAMPLE_COUNT_1_BIT;
    uint8_t     _mipmapLevel = 1;
    std::string _debugName;
    int         _poolId = -1;
};

struct Buffer : public nvvk::Buffer
{
    Buffer(int poolID) : _poolId(poolID) {};
    int                    _poolId = -1;
    VkDescriptorBufferInfo descriptor;
    VkBufferUsageFlags _usageFlags;
    VkDeviceSize _size;
    VkDeviceSize _range = VK_WHOLE_SIZE;
    std::string _debugName;
};

template <typename T>
class BasePool
{
   public:
    void init(uint32_t poolSize, nvvk::ResourceAllocatorVma* allocator)
    {
        _allocator = allocator;
        _objs.resize(poolSize);
        _freeIndices.resize(poolSize);
        for (uint32_t i = 0; i < poolSize; ++i)
        {
            _freeIndices[i] = i;
        }
    }

    void deinit()
    {
        for (auto& obj : _objs)
        {
            if (obj && obj->_poolId >= 0)
            {
                _allocator->destroy(*obj);
                obj->_poolId = -1;
                delete (obj);
            }
        }
    }

    template <typename TT>
    T* alloc()
    {
        NV_ASSERT(_availableIndex < _objs.size());
        uint32_t index       = _freeIndices[_availableIndex++];
        _objs[index]         = static_cast<T*>(new TT(index));
        return _objs[index];
    };

    std::vector<T*>             _objs;
    std::vector<uint32_t>       _freeIndices;
    uint32_t                    _availableIndex = 0;
    nvvk::ResourceAllocatorVma* _allocator;
};

class TexturePool : public BasePool<Texture>
{
   public:
    void free(Texture* obj)
    {
        if (obj == nullptr || !(obj->_poolId < _objs.size() && obj->_poolId >= 0))
        {
            return;
        }
        _freeIndices[--_availableIndex] = obj->_poolId;
        _allocator->destroy(*obj);
        obj->_poolId = -1;
    };
};

class BufferPool : public BasePool<Buffer>
{
   public:
    void free(Buffer* obj)
    {
        if (obj == nullptr || !(obj->_poolId < _objs.size() && obj->_poolId >= 0))
        {
            return;
        }
        _freeIndices[--_availableIndex] = obj->_poolId;
        _allocator->destroy(*obj);
        obj->_poolId = -1;
    };
};
} // namespace Play
#endif // RESOURCE_H