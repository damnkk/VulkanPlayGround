#ifndef PLAYALLOCATOR_H
#define PLAYALLOCATOR_H
#include "nvvk/resource_allocator.hpp"
#include "nvvk/sampler_pool.hpp"
#include "nvvk/staging.hpp"
#include <filesystem>
#include <mutex>

namespace Play
{
class Buffer;
class Texture;
class PlayResourceManager;

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

class PlayResourceManager : public nvvk::ResourceAllocatorExport, public nvvk::StagingUploader, public nvvk::SamplerPool
{
public:
    static PlayResourceManager&           Instance();
    static nvvk::ResourceAllocatorExport* GetAsAllocator();
    static nvvk::StagingUploader*         GetAsStagingUploader();
    static nvvk::SamplerPool*             GetAsSamplerPool();
    PlayResourceManager() = default;
    ~PlayResourceManager() {}
    void            initialize();
    void            deInit();
    VkCommandBuffer getTempCommandBuffer();
    void            submitAndWaitTempCmdBuffer(VkCommandBuffer cmd);

private:
    VkCommandPool _tempCmdPool{VK_NULL_HANDLE};
};

} // namespace Play
#endif // PLAYALLOCATOR_H