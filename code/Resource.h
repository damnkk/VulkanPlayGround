#ifndef RESOURCE_H
#define RESOURCE_H
#include "stdint.h"
#include "vector"
#include "nvvk/memallocator_vma_vk.hpp"

struct Texture : public nvvk::Texture
{
    VkFormat    _format      = VK_FORMAT_UNDEFINED;
    VkImageType _type        = VK_IMAGE_TYPE_2D;
    uint8_t     _mipmapLevel = 1;
    std::string _debugName;
    uint32_t    _poolId;
};

struct Buffer : public nvvk::Buffer
{
    uint32_t _poolId;
};

struct Mesh
{
    uint32_t _vertexOffset  = 0;
    uint32_t _vertexCount   = 0;
    uint32_t _indexOffset   = 0;
    uint32_t _indexCount    = 0;
    int      _materialIndex = -1;
};

struct Material
{
    std::vector<Texture> _textures;
};

template <typename T>
class BasePool
{
   public:
    void init(uint32_t poolSize, nvvk::ResourceAllocatorVma* allocator)
    {
        _allocator = allocator;
        _objs.reserve(poolSize);
        _freeIndices.reserve(poolSize);
        _freeIndices.resize(poolSize);
        for (uint32_t i = 0; i < poolSize; ++i)
        {
            _freeIndices[i] = i;
        }
    }
    T& alloc()
    {
        assert(_availableIndex < _objs.size());
        uint32_t index       = _freeIndices[_availableIndex++];
        _objs[index]._poolId = index;
        return _objs[index];
    };

    std::vector<T>              _objs;
    std::vector<uint32_t>       _freeIndices;
    uint32_t                    _availableIndex = 0;
    nvvk::ResourceAllocatorVma* _allocator;
};

class TexturePool : public BasePool<Texture>
{
   public:
    void free(Texture& obj)
    {
        assert(obj._poolId < _objs.size() && obj._poolId >= 0);
        _freeIndices[--_availableIndex] = obj._poolId;
        _allocator->destroy(obj);
        obj._poolId = -1;
    };
};

class BufferPool : public BasePool<nvvk::Buffer>
{
   public:
    void free(Buffer& obj)
    {
        assert(obj._poolId < _objs.size() && obj._poolId >= 0);
        _freeIndices[--_availableIndex] = obj._poolId;
        _allocator->destroy(obj);
        obj._poolId = -1;
    };
};
#endif // RESOURCE_H