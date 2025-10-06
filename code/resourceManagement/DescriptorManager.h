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

class DescriptorManager
{
public:
    DescriptorManager() = default;
    ~DescriptorManager();
    void init(VkPhysicalDevice physicalDevice, VkDevice device);
    void deinit();
    void updateDescSetBindingOffset(DescriptorSetManager* manager);

    void updateDescriptor(DescriptorEnum setEnum, uint32_t bindingIdx,
                          VkDescriptorType descriptorType, uint32_t descriptorCount,
                          Buffer* buffers);

    void updateDescriptor(DescriptorEnum setIdx, uint32_t bindingIdx,
                          VkDescriptorType descriptorType, uint32_t descriptorCount,
                          nvvk::Image* imageInfos);

    void updateDescriptor(DescriptorEnum setIdx, uint32_t bindingIdx,
                          VkDescriptorType descriptorType, uint32_t descriptorCount,
                          const VkBufferView* bufferViews);

    void updateDescriptor(DescriptorEnum setEnum, uint32_t bindingIdx,
                          VkDescriptorType descriptorType, uint32_t descriptorCount,
                          nvvk::AccelerationStructure* accels);

    void cmdBindDescriptorBuffers(VkCommandBuffer cmdBuf, PlayProgram* program);

protected:
    size_t getDescriptorSize(VkDescriptorType descriptorType);

private:
    Buffer*                                       _descBuffer = nullptr;
    VkDevice                                      _device;
    VkPhysicalDevice                              _physicalDevice;
    VkPhysicalDeviceDescriptorBufferPropertiesEXT _descriptorBufferProperties;
    std::array<std::unordered_map<uint32_t, size_t>, MAX_DESCRIPTOR_SETS::value>
        _descriptorOffsetInfo;
};

} // namespace Play

#endif // DESCRIPTOR_MANAGER_H