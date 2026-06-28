#ifndef DESCRIPTOR_MANAGER_H
#define DESCRIPTOR_MANAGER_H
#include "Resource.h"
#include "utils.hpp"
#include "core/RefCounted.h"
#include <nvvk/descriptors.hpp>
namespace Play
{
enum class DescriptorEnum : uint32_t
{
    eGlobalDescriptorSet,
    eSceneDescriptorSet,
    eFrameDescriptorSet,
    ePerPassDescriptorSet,
    eDrawObjectDescriptorSet,
    eCount
};
struct BindInfo
{
    uint32_t           bindingIdx;
    uint32_t           descriptorCount;
    VkDescriptorType   descriptorType;
    VkShaderStageFlags shaderStageFlags;
};

union DescriptorInfo
{
    VkDescriptorBufferInfo     buffer;
    VkDescriptorImageInfo      image;
    VkAccelerationStructureKHR accel;
};

class DescriptorSetBindings : public nvvk::DescriptorBindings
{
public:
    DescriptorSetBindings();
    explicit DescriptorSetBindings(DescriptorEnum setSlot);
    ~DescriptorSetBindings();
    void reset(DescriptorEnum setSlot = DescriptorEnum::eCount);
    DescriptorSetBindings& addBinding(uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType,
                                      VkShaderStageFlags shaderStageFlags);
    DescriptorSetBindings& addBinding(const BindInfo& bindingInfo);

    void setDescriptorSetSlot(DescriptorEnum setSlot)
    {
        _setSlot = setSlot;
    }

    DescriptorEnum getDescriptorSetSlot() const
    {
        return _setSlot;
    }

    void setDescInfo(uint32_t bindingIdx, const nvvk::Buffer& buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);

    void setDescInfo(uint32_t bindingIdx, const nvvk::Image& image);
    void setDescInfo(uint32_t bindingIdx, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);
    void setDescInfo(uint32_t bindingIdx, const VkDescriptorBufferInfo& bufferInfo);
    void setDescInfo(uint32_t bindingIdx, VkImageView imageView, VkImageLayout imageLayout, VkSampler sampler = nullptr);
    void setDescInfo(uint32_t bindingIdx, const VkDescriptorImageInfo& imageInfo);
    void setDescInfo(uint32_t bindingIdx, VkAccelerationStructureKHR accel);

    // writeSet.descriptorCount many elements
    void setDescInfo(uint32_t bindingIdx, const nvvk::Buffer* buffers, uint32_t count); // offset 0 and VK_WHOLE_SIZE
    void setDescInfo(uint32_t bindingIdx, const nvvk::Image* images, uint32_t count);
    void setDescInfo(uint32_t bindingIdx, const VkDescriptorBufferInfo* bufferInfos, uint32_t count);
    void setDescInfo(uint32_t bindingIdx, const VkDescriptorImageInfo* imageInfos, uint32_t count);
    void setDescInfo(uint32_t bindingIdx, const VkAccelerationStructureKHR* accels, uint32_t count);

    int                   descriptorOffset(uint32_t bindingIdx);
    uint64_t              getBindingsHash(); // flush dirty flag
    uint64_t              getDescsetLayoutHash();
    VkDescriptorSetLayout finalizeLayout(); // flush recorded flag
    VkDescriptorSet       getOrAcquireDescriptorSet(DescriptorEnum setSlot = DescriptorEnum::eCount);

    bool isLayoutDirty() const
    {
        return _setLayoutDirty;
    }

    bool isDescriptorInfoDirty() const
    {
        return _descInfoDirty != 0;
    }

    bool isDescriptorSetDirty() const
    {
        return _descriptorSetDirty;
    }

    const VkDescriptorSetLayout& getSetLayout()
    {
        return _layout;
    }

    VkDescriptorSet getCachedDescriptorSet() const
    {
        return _cachedDescriptorSet;
    }

    const std::vector<DescriptorInfo>& getDescriptorInfos()
    {
        return _descInfos;
    }

protected:
    std::vector<BindInfo>       _bindingInfos;
    uint64_t                    _setBindingHash = 0;
    VkDescriptorSetLayout       _layout         = VK_NULL_HANDLE;
    std::vector<DescriptorInfo> _descInfos;

private:
    void markLayoutDirty();
    void markDescriptorInfoDirty();

    DescriptorEnum  _setSlot             = DescriptorEnum::eCount;
    VkDescriptorSet _cachedDescriptorSet = VK_NULL_HANDLE;
    bool            _setLayoutDirty      = true; // layout changing state
    uint8_t         _descInfoDirty       = 0;    // descinfo changing state | bit0: binding changed, bit1: constant range changed
    bool            _descriptorSetDirty  = true;
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
    // void updateDescSetBindingOffset(DescriptorSetManager* manager);

    void updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount, Buffer* buffers);

    void updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount, nvvk::Image* imageInfos);

    void updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
                          const VkBufferView* bufferViews);

    void updateDescriptor(uint32_t setIdx, uint32_t bindingIdx, VkDescriptorType descriptorType, uint32_t descriptorCount,
                          nvvk::AccelerationStructure* accels);


protected:
    size_t getDescriptorSize(VkDescriptorType descriptorType);

private:
    RefPtr<Buffer>                                                                                      _descBuffer;
    VkDevice                                                                                            _device;
    VkPhysicalDevice                                                                                    _physicalDevice;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT                                                   _descriptorBufferProperties;
    std::array<std::unordered_map<uint32_t, size_t>, static_cast<uint32_t>(DescriptorEnum::eCount)> _descriptorOffsetInfo;
};

struct CommonDescriptorSet
{
    VkDescriptorSet       set;
    VkDescriptorSetLayout layout;
};

class DescriptorSetCache
{
public:
    DescriptorSetCache() {}
    ~DescriptorSetCache();
    void                deInit();
    VkDescriptorSet     requestDescriptorSet(DescriptorSetBindings* setManager, uint32_t setIdx);
    CommonDescriptorSet getEngineDescriptorSet()
    {
        return _globalDescriptorSet;
    }
    CommonDescriptorSet getSceneDescriptorSet()
    {
        return _sceneDescriptorSet;
    }
    CommonDescriptorSet getFrameDescriptorSet()
    {
        return _frameDescriptorSet;
    }

    void initGlobalDescriptorSets(nvvk::DescriptorBindings& setBindings);
    void initFrameDescriptorSets(nvvk::DescriptorBindings& setBindings);
    void initSceneDescriptorSets(nvvk::DescriptorBindings& setBindings);

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
    VkDescriptorSet                                          createDescriptorSet(CacheNode& cacheNode, DescriptorSetBindings* setManager);
    CacheNode::CachedSet                                     createDescriptorSetImplement(CacheNode& cacheNode, DescriptorSetBindings* setManager);
    std::unordered_map<uint64_t, std::shared_ptr<CacheNode>> _descriptorPoolMap;
    CommonDescriptorSet                                      _globalDescriptorSet  = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDescriptorPool                                         _globalDescriptorPool = VK_NULL_HANDLE;
    CommonDescriptorSet                                      _sceneDescriptorSet   = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDescriptorPool                                         _sceneDescriptorPool  = VK_NULL_HANDLE;
    CommonDescriptorSet                                      _frameDescriptorSet   = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDescriptorPool                                         _frameDescriptorPool  = VK_NULL_HANDLE;
};

} // namespace Play

#endif // DESCRIPTOR_MANAGER_H