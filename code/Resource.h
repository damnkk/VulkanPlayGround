#ifndef RESOURCE_H
#define RESOURCE_H
#include "stdint.h"
#include "vector"

#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"

template <typename T>
class BasePool
{
    std::vector<T>        _objs;
    std::vector<uint32_t> _freeIndices;
    uint32_t              _nextIndex = 0;
};

class TexturePool : public BasePool<nvvk::Texture>
{
};

class BufferPool : public BasePool<nvvk::Buffer>
{
};

struct Texture : public nvvk::Texture
{
    VkFormat    _format      = VK_FORMAT_UNDEFINED;
    VkImageType _type        = VK_IMAGE_TYPE_2D;
    uint8_t     _mipmapLevel = 1;
    std::string _debugName;
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

#endif // RESOURCE_H