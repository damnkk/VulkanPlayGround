#ifndef PLAYALLOCATOR_H
#define PLAYALLOCATOR_H
#include "nvvk/resource_allocator.hpp"
#include "nvvk/sampler_pool.hpp"
#include "nvvk/staging.hpp"
#include <filesystem>
#include <mutex>

namespace Play
{
namespace RDG
{
class RDGTexturePool;
class RDGBufferPool;
} // namespace RDG
class Buffer;
class Texture;
class PlayResourceManager;
class PlayElement;
template <typename T>
class BasePool
{
public:
    void init(uint32_t poolSize, PlayResourceManager* manager)
    {
        _manager = manager;
        _objs.resize(poolSize);
        _freeIndices.resize(poolSize);
        for (uint32_t i = 0; i < poolSize; ++i)
        {
            _freeIndices[i] = i;
        }
    }

    virtual void deinit() {};

    std::vector<T*>       _objs;
    std::vector<uint32_t> _freeIndices;
    uint32_t              _availableIndex = 0;
    PlayResourceManager*  _manager;
    std::mutex            _mutex;
};

class TexturePool : public BasePool<Texture>
{
public:
    static TexturePool& Instance();

    // 释放纹理对象
    void free(Texture* obj);

    [[nodiscard]] Texture* alloc(VkImageCreateInfo* imgInfo, VkImageViewCreateInfo* viewInfo);

    // 通用2D纹理分配
    [[nodiscard]] Texture* alloc(uint32_t width, uint32_t height, VkFormat format,
                                 VkImageUsageFlags     usage,
                                 VkImageLayout         layout    = VK_IMAGE_LAYOUT_UNDEFINED,
                                 uint32_t              mipLevels = 1,
                                 VkSampleCountFlagBits samples   = VK_SAMPLE_COUNT_1_BIT);

    // 3D纹理分配
    [[nodiscard]] Texture* alloc(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                                 VkImageUsageFlags usage, VkImageLayout initialLayout,
                                 uint32_t mipLevels = 1);

    // 立方体纹理分配
    [[nodiscard]] Texture* allocCube(uint32_t size, VkFormat format, VkImageUsageFlags usage,
                                     VkImageLayout initialLayout, uint32_t mipLevels = 1);

    // 从像素数据分配2D纹理
    [[nodiscard]] Texture* alloc(const void* data, size_t dataSize, uint32_t width, uint32_t height,
                                 VkFormat format, VkImageUsageFlags usage, uint32_t mipLevels = 1);

    // 分配带采样器的2D纹理
    [[nodiscard]] Texture* alloc(uint32_t width, uint32_t height, VkFormat format,
                                 VkImageUsageFlags usage, VkImageLayout initialLayout,
                                 VkFilter filter, VkSamplerAddressMode addressMode,
                                 uint32_t mipLevels = 1);

    // 通过文件路径创建2D纹理
    [[nodiscard]] Texture* alloc(
        const std::filesystem::path& imagePath,
        VkImageLayout                finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        uint32_t                     mipLevels   = 1)
    {
        return nullptr;
    }

    void deinit() override;

protected:
    friend class Texture;
    // 分配一个空纹理对象
    Texture* alloc();
};

class BufferPool : public BasePool<Buffer>
{
public:
    static BufferPool& Instance();

    // 按大小和用法分配Buffer
    Buffer* alloc(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    // 按初始数据分配Buffer
    Buffer* alloc(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties);

    Buffer* alloc(VkBufferCreateInfo& bufferInfo);

    // 通过文件路径分配Buffer（如用于上传文件内容到GPU）
    // Buffer* alloc(const std::filesystem::path& filePath, VkBufferUsageFlags usage,
    // VkMemoryPropertyFlags properties);

    void free(Buffer* obj);

    void deinit() override;

protected:
    friend class Buffer;
    // 分配一个空Buffer对象
    Buffer* alloc();
};

class PlayResourceManager : public nvvk::ResourceAllocatorExport,
                            public nvvk::StagingUploader,
                            public nvvk::SamplerPool
{
public:
    static PlayResourceManager&           Instance();
    static nvvk::ResourceAllocatorExport* GetAsAllocator();
    static nvvk::StagingUploader*         GetAsStagingUploader();
    static nvvk::SamplerPool*             GetAsSamplerPool();
    PlayResourceManager() = default;
    ~PlayResourceManager() {}
    void            initialize(PlayElement* element);
    void            deInit();
    VkCommandBuffer getTempCommandBuffer();
    void            submitAndWaitTempCmdBuffer(VkCommandBuffer cmd);

private:
    friend class TexturePool;
    friend class BufferPool;
    VkCommandPool _tempCmdPool{VK_NULL_HANDLE};

    PlayElement* _element{nullptr};
};

} // namespace Play
#endif // PLAYALLOCATOR_H