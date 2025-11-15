#ifndef DESCRIPTOR_MANAGER_H
#define DESCRIPTOR_MANAGER_H
#include "Resource.h"
#include "utils.hpp"
namespace Play
{
class DescriptorSetManager;
class PlayProgram;
enum class DescriptorEnum : uint32_t
{
    eGlobalDescriptorSet,
    eSceneDescriptorSet,
    eFrameDescriptorSet,
    ePerPassDescriptorSet,
    eDrawObjectDescriptorSet,
    eCount
};
// 256 descriptor slot for global
const size_t GLOBAL_DESCRIPTOR_SET_OFFSET = 0;
// 2048 descriptor slot for scene
const size_t PER_SCENE_DESCRIPTOR_SET_OFFSET = 256 * 32;
// 512 descriptor slot for frame
const size_t PER_FRAME_DESCRIPTOR_SET_OFFSET = (256 + 2048) * 32;
// 256 descriptor slot for pass
const size_t PER_PASS_DESCRIPTOR_SET_OFFSET = (256 + 2048 + 512) * 32;
// 1024 descriptor slot for draw object
const size_t PER_DRAW_OBJECT_DESCRIPTOR_SET_OFFSET = 3072 * 32;

class DescriptorBufferManagerExt
{
public:
    DescriptorBufferManagerExt() = default;
    ~DescriptorBufferManagerExt();
    void init(VkPhysicalDevice physicalDevice, VkDevice device);
    void deinit();
    void updateDescSetBindingOffset(DescriptorSetManager* manager);

    void updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType,
                          uint32_t descriptorCount, Buffer* buffers);

    void updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType,
                          uint32_t descriptorCount, nvvk::Image* imageInfos);

    void updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType,
                          uint32_t descriptorCount, const VkBufferView* bufferViews);

    void updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType,
                          uint32_t descriptorCount, nvvk::AccelerationStructure* accels);

    void cmdBindDescriptorBuffers(VkCommandBuffer cmdBuf, PlayProgram* program);

protected:
    size_t getDescriptorSize(VkDescriptorType descriptorType);

private:
    Buffer*                                       _descBuffer = nullptr;
    VkDevice                                      _device;
    VkPhysicalDevice                              _physicalDevice;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT _descriptorBufferProperties;
    std::array<std::unordered_map<uint32_t, size_t>, static_cast<uint32_t>(DescriptorEnum::eCount)>
        _descriptorOffsetInfo;
};

class DescriptorSetCache
{
public:
    DescriptorSetCache(PlayElement* element) : _element(element) {}
    ~DescriptorSetCache();
    void            deInit();
    VkDescriptorSet requestDescriptorSet(DescriptorSetManager& setManager, uint32_t setIdx);

private:
    struct CacheNode
    {
        struct PoolNode
        {
            static const uint32_t maxSetPerPool = 32;
            VkDescriptorPool      pool;
            uint32_t              availableCount = maxSetPerPool;
            uint64_t              lastUsedFrame  = 0;
        };
        struct CachedSet
        {
            uint32_t        parentPoolIndex;
            VkDescriptorSet descriptorSet;
        };
        CachedSet createCachedSet()
        {
            CachedSet newSet;
            newSet.parentPoolIndex = pools.size() - 1;
            newSet.descriptorSet   = VK_NULL_HANDLE;
            return newSet;
        }
        std::unordered_map<size_t, CachedSet> descriptorSetMap;
        std::vector<PoolNode>                 pools;
    };
    VkDescriptorSet      createDescriptorSet(CacheNode& cacheNode, DescriptorSetManager& setManager,
                                             uint32_t setIdx);
    CacheNode::CachedSet createDescriptorSetImplement(CacheNode&            cacheNode,
                                                      DescriptorSetManager& setManager,
                                                      uint32_t              setIdx);
    std::unordered_map<uint64_t, CacheNode> _descriptorPoolMap;
    VkDescriptorSet                         _globalDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet                         _sceneDescriptorSet  = VK_NULL_HANDLE;
    VkDescriptorSet                         _frameDescriptorSet  = VK_NULL_HANDLE;
    Play::PlayElement*                      _element             = nullptr;
};

} // namespace Play

#endif // DESCRIPTOR_MANAGER_H