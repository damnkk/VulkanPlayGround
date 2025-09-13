
#include "PlayProgram.h"
#include "nvvk/check_error.hpp"
namespace Play
{
DescriptorSetManager::DescriptorSetManager(VkDevice device) : _vkDevice(device) {}

DescriptorSetManager::~DescriptorSetManager() {}
DescriptorSetManager& DescriptorSetManager::addBinding(const BindInfo& bindingInfo)
{
    _recordState = false;
    for (int i = 0; i < _bindingInfos.size(); i++)
    {
        if (_bindingInfos[i].setIdx == bindingInfo.setIdx &&
            _bindingInfos[i].bindingIdx == bindingInfo.bindingIdx)
        {
            if (_bindingInfos[i].descriptorType != bindingInfo.descriptorType)
            {
                LOGE("Descriptor type mismatch");
                return *this;
            }
            _bindingInfos[i].descriptorCount += bindingInfo.descriptorCount;
            _bindingInfos[i].pipelineStageFlags |= bindingInfo.pipelineStageFlags;
            return *this;
        }
    }
    _bindingInfos.push_back(bindingInfo);
    return *this;
}

DescriptorSetManager& DescriptorSetManager::addBinding(uint32_t setIdx, uint32_t bindingIdx,
                                                       uint32_t              descriptorCount,
                                                       VkDescriptorType      descriptorType,
                                                       VkPipelineStageFlags2 pipelineStageFlags)
{
    _recordState         = false;
    BindInfo bindingInfo = {setIdx, bindingIdx, descriptorCount, descriptorType,
                            pipelineStageFlags};
    return addBinding(bindingInfo);
}

DescriptorSetManager& DescriptorSetManager::initLayout()
{
    for (int i = 0; i < MAX_DESCRIPTOR_SETS::value; ++i)
    {
        _descBindSet[i].createDescriptorSetLayout(_vkDevice, 0, &_descSetLayouts[i]);
    }
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCreateInfo.setLayoutCount         = static_cast<uint32_t>(_descSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts            = _descSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(_constantRanges.size());
    pipelineLayoutCreateInfo.pPushConstantRanges    = _constantRanges.data();
    NVVK_CHECK(
        vkCreatePipelineLayout(_vkDevice, &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout));
    return *this;
}

DescriptorSetManager& DescriptorSetManager::addConstantRange(uint32_t size, uint32_t offset,
                                                             VkPipelineStageFlags2 stage)
{
    _recordState = false;
    _constantRanges.emplace_back(stage, offset, size);
    return *this;
}

bool DescriptorSetManager::finish()
{
    _recordState = true;
    for (auto& layout : _descSetLayouts)
    {
        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(_vkDevice, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }
    initLayout();
    return _recordState;
}

VkPipelineLayout DescriptorSetManager::getPipelineLayout() const
{
    if (!_recordState)
    {
        LOGE("Pipeline layout is not created, check finish() is called");
        return VK_NULL_HANDLE;
    }
    if (_pipelineLayout == VK_NULL_HANDLE)
    {
        LOGE("Pipeline layout is not created");
    }
    return _pipelineLayout;
}

} // namespace Play