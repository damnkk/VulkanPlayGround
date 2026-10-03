#ifndef DESCRIPTOR_MANAGER_H
#define DESCRIPTOR_MANAGER_H
#include "resourceManagement/vulkan/resources/Resource.h"
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
    bool               operator==(const BindInfo&) const = default;
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
    void                   reset();
    DescriptorSetBindings& addBinding(uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType,
                                      VkShaderStageFlags shaderStageFlags);
    DescriptorSetBindings& addBinding(const BindInfo& bindingInfo);

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

    int descriptorOffset(uint32_t bindingIdx);
    // Finalize binding order and CPU descriptor storage before assigning resources.
    void finalizeBindings();

    const std::vector<DescriptorInfo>& getDescriptorInfos()
    {
        return _descInfos;
    }

private:
    friend class DescriptorSetCache;
    std::vector<uint64_t> getDescriptorKey() const;

    std::vector<BindInfo>       _bindingInfos;
    std::vector<DescriptorInfo> _descInfos;
    bool                        _bindingsDirty = true;
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
    RefPtr<Buffer>                                                                                  _descBuffer;
    VkDevice                                                                                        _device;
    VkPhysicalDevice                                                                                _physicalDevice;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT                                                   _descriptorBufferProperties;
    std::array<std::unordered_map<uint32_t, size_t>, static_cast<uint32_t>(DescriptorEnum::eCount)> _descriptorOffsetInfo;
};

struct CommonDescriptorSet
{
    VkDescriptorSet       set    = VK_NULL_HANDLE;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
};

class DescriptorSetCache
{
public:
    DescriptorSetCache() {}
    ~DescriptorSetCache();
    // Cache immutable sets by layout and bound resources. Empty bindings require no set.
    CommonDescriptorSet getOrCreateDescriptorSet(DescriptorSetBindings& bindings);
    CommonDescriptorSet getEngineDescriptorSet()
    {
        return _globalDescriptorSet;
    }
    CommonDescriptorSet getSceneDescriptorSet();
    uint32_t            getSceneDescriptorSetCount() const
    {
        return static_cast<uint32_t>(_sceneDescriptorSets.size());
    }
    CommonDescriptorSet getFrameDescriptorSet()
    {
        return _frameDescriptorSet;
    }

    void initGlobalDescriptorSets(nvvk::DescriptorBindings& setBindings);
    void initFrameDescriptorSets(nvvk::DescriptorBindings& setBindings);
    void initSceneDescriptorSets(nvvk::DescriptorBindings& setBindings);

private:
    VkDescriptorSetLayout getOrCreateDescriptorSetLayout(DescriptorSetBindings& bindings);
    struct CacheNode
    {
        struct PoolNode
        {
            static const uint32_t maxSetPerPool = 32;
            VkDescriptorPool      pool;
            uint32_t              availableCount = maxSetPerPool;
        };
        struct CachedSet
        {
            std::vector<uint64_t> descriptorKey;
            VkDescriptorSet       descriptorSet = VK_NULL_HANDLE;
        };
        std::unordered_map<size_t, std::vector<CachedSet>> descriptorSetMap;
        std::vector<PoolNode>                              pools;
    };
    VkDescriptorSet createDescriptorSet(CacheNode& cacheNode, VkDescriptorSetLayout layout, DescriptorSetBindings& bindings);
    struct LayoutEntry
    {
        std::vector<BindInfo> bindings;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    };
    std::unordered_map<uint64_t, std::vector<LayoutEntry>> _descriptorLayoutMap;
    std::unordered_map<VkDescriptorSetLayout, CacheNode>   _descriptorPoolMap;
    CommonDescriptorSet                                    _globalDescriptorSet  = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDescriptorPool                                       _globalDescriptorPool = VK_NULL_HANDLE;
    CommonDescriptorSet                                    _sceneDescriptorSet   = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::vector<VkDescriptorSet>                           _sceneDescriptorSets;
    VkDescriptorPool                                       _sceneDescriptorPool = VK_NULL_HANDLE;
    CommonDescriptorSet                                    _frameDescriptorSet  = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDescriptorPool                                       _frameDescriptorPool = VK_NULL_HANDLE;
};

} // namespace Play

#endif // DESCRIPTOR_MANAGER_H
