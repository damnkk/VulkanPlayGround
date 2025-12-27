
#include "PlayProgram.h"
#include "nvvk/check_error.hpp"
#include "spirv_reflect.h"
#include "ShaderManager.hpp"
#include "crc32c/crc32c.h"
#include "VulkanDriver.h"
namespace Play
{

PushConstantManager::PushConstantManager()
{
    _maxSize = vkDriver->_physicalDeviceProperties2.properties.limits.maxPushConstantsSize;
    _constantData.resize(_maxSize, 0);
}

const std::vector<VkPushConstantRange>& PushConstantManager::getRanges() const
{
    return _ranges;
}

uint32_t PushConstantManager::getMaxSize() const
{
    return _maxSize;
}

void PushConstantManager::clear()
{
    _currOffset = 0;
    _ranges.clear();
    std::fill(_constantData.begin(), _constantData.end(), 0);
    _typeMap.clear();
}

DescriptorSetManager::DescriptorSetManager(VkDevice device) : _vkDevice(device) {}

DescriptorSetManager::~DescriptorSetManager() {}
void DescriptorSetManager::deinit()
{
    if (_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(_vkDevice, _pipelineLayout, nullptr);
        _pipelineLayout = VK_NULL_HANDLE;
    }
    for (size_t i = (size_t) DescriptorEnum::ePerPassDescriptorSet; i < (size_t) DescriptorEnum::eCount; ++i)
    {
        auto& layout = _descSetLayouts[i];
        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(_vkDevice, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }
}
DescriptorSetManager& DescriptorSetManager::addBinding(const BindInfo& bindingInfo)
{
    if (_isRecorded) return *this;
    for (int i = 0; i < _bindingInfos.size(); i++)
    {
        if (_bindingInfos[i].setIdx == bindingInfo.setIdx && _bindingInfos[i].bindingIdx == bindingInfo.bindingIdx)
        {
            if (_bindingInfos[i].descriptorType != bindingInfo.descriptorType)
            {
                LOGE("Descriptor type mismatch");
                return *this;
            }
            _bindingInfos[i].shaderStageFlags |= bindingInfo.shaderStageFlags;
            _bindingInfos[i].descriptorCount += bindingInfo.descriptorCount;
            return *this;
        }
    }
    _bindingInfos.push_back(bindingInfo);
    return *this;
}

DescriptorSetManager& DescriptorSetManager::addBinding(uint32_t setIdx, uint32_t bindingIdx, uint32_t descriptorCount,
                                                       VkDescriptorType descriptorType, VkShaderStageFlags shaderStageFlags)
{
    if (_isRecorded) return *this;
    BindInfo bindingInfo = {setIdx, bindingIdx, descriptorCount, descriptorType, shaderStageFlags};
    return addBinding(bindingInfo);
}

DescriptorSetManager& DescriptorSetManager::initLayout()
{
    for (int i = uint32_t(DescriptorEnum::ePerPassDescriptorSet); i < uint32_t(DescriptorEnum::eCount); ++i)
    {
        _descBindSet[i].createDescriptorSetLayout(_vkDevice, 0, &_descSetLayouts[i]);
    }
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCreateInfo.setLayoutCount         = static_cast<uint32_t>(_descSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts            = _descSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(_constantRanges.getRanges().size());
    pipelineLayoutCreateInfo.pPushConstantRanges    = _constantRanges.getRanges().data();
    NVVK_CHECK(vkCreatePipelineLayout(_vkDevice, &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout));
    return *this;
}

bool DescriptorSetManager::finalizeLayout()
{
    if (!(_isRecorded ^ true))
    {
        return _isRecorded;
    }
    for (auto& layout : _descSetLayouts)
    {
        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(_vkDevice, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }
    _descSetLayouts[uint32_t(DescriptorEnum::eGlobalDescriptorSet)] = vkDriver->getDescriptorSetCache()->getEngineDescriptorSet().layout;
    _descSetLayouts[uint32_t(DescriptorEnum::eSceneDescriptorSet)]  = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().layout;
    _descSetLayouts[uint32_t(DescriptorEnum::eFrameDescriptorSet)]  = vkDriver->getDescriptorSetCache()->getFrameDescriptorSet().layout;
    for (const auto& bindings : _bindingInfos)
    {
        assert(bindings.setIdx <= uint32_t(DescriptorEnum::eCount));
        auto& set = _descBindSet[bindings.setIdx];
        set.addBinding(bindings.bindingIdx, bindings.descriptorType, bindings.descriptorCount, bindings.shaderStageFlags);
    }
    initLayout();
    uint32_t descriptorCount = 0;
    std::for_each(_bindingInfos.begin(), _bindingInfos.end(),
                  [&](const BindInfo& info)
                  {
                      if (info.setIdx == uint32_t(DescriptorEnum::ePerPassDescriptorSet)) descriptorCount += info.descriptorCount;
                  });
    _descInfos[0].resize(descriptorCount);
    descriptorCount = 0;
    std::for_each(_bindingInfos.begin(), _bindingInfos.end(),
                  [&](const BindInfo& info)
                  {
                      if (info.setIdx == uint32_t(DescriptorEnum::eDrawObjectDescriptorSet)) descriptorCount += info.descriptorCount;
                  });
    _descInfos[1].resize(descriptorCount);

    return _isRecorded = true;
}

VkPipelineLayout DescriptorSetManager::getPipelineLayout() const
{
    if (!_isRecorded)
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

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::Buffer& buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].buffer;
    if (bufferInfo.buffer == buffer.buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    bufferInfo.buffer = buffer.buffer;
    bufferInfo.offset = offset;
    bufferInfo.range  = range;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::AccelerationStructure& accel)
{
    auto& accelInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].accel;
    if (accelInfo == accel.accel)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    accelInfo = accel.accel;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::Image& image)
{
    auto& imageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)];
    if (imageInfo.image.imageLayout == image.descriptor.imageLayout && imageInfo.image.imageView == image.descriptor.imageView &&
        imageInfo.image.sampler == image.descriptor.sampler)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    imageInfo.image = image.descriptor;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].buffer;
    if (bufferInfo.buffer == buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range  = range;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkDescriptorBufferInfo& bufferInfo)
{
    auto& destBufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].buffer;
    if (destBufferInfo.buffer == bufferInfo.buffer && destBufferInfo.offset == bufferInfo.offset && destBufferInfo.range == bufferInfo.range)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    destBufferInfo = bufferInfo;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, VkImageView imageView, VkImageLayout imageLayout, VkSampler sampler)
{
    auto& imageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].image;
    if (imageInfo.imageLayout == imageLayout && imageInfo.imageView == imageView && imageInfo.sampler == sampler)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    imageInfo.imageLayout = imageLayout;
    imageInfo.imageView   = imageView;
    imageInfo.sampler     = sampler;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkDescriptorImageInfo& imageInfo)
{
    auto& destImageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].image;
    if (destImageInfo.imageLayout == imageInfo.imageLayout && destImageInfo.imageView == imageInfo.imageView &&
        destImageInfo.sampler == imageInfo.sampler)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    destImageInfo = imageInfo;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, VkAccelerationStructureKHR accel)
{
    auto& accelInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].accel;
    if (accelInfo == accel)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    accelInfo = accel;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::Buffer* buffers, uint32_t count)
{
    auto& bufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].buffer;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (bufferInfo.buffer == buffers[i].buffer)
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        bufferInfo.buffer = buffers[i].buffer;
    }
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::AccelerationStructure* accels, uint32_t count)
{
    auto& accelInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].accel;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (accelInfo == accels[i].accel)
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        accelInfo = accels[i].accel;
    }
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::Image* images, uint32_t count)
{
    auto& imageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].image;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (imageInfo.imageLayout == images[i].descriptor.imageLayout && imageInfo.imageView == images[i].descriptor.imageView &&
            imageInfo.sampler == images[i].descriptor.sampler)
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        imageInfo = images[i].descriptor;
    }
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkDescriptorBufferInfo* bufferInfos, uint32_t count)
{
    auto& destBufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].buffer;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (destBufferInfo.buffer == bufferInfos[i].buffer && destBufferInfo.offset == bufferInfos[i].offset &&
            destBufferInfo.range == bufferInfos[i].range)
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        destBufferInfo = bufferInfos[i];
    }
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkDescriptorImageInfo* imageInfos, uint32_t count)
{
    auto& imageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].image;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (imageInfo.imageLayout == imageInfos[i].imageLayout && imageInfo.imageView == imageInfos[i].imageView &&
            imageInfo.sampler == imageInfos[i].sampler)
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        imageInfo = imageInfos[i];
    }
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkAccelerationStructureKHR* accels, uint32_t count)
{
    auto& accelInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)][descriptorOffset(setIdx, bindingIdx)].accel;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (accelInfo == accels[i])
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        accelInfo = accels[i];
    }
}

int DescriptorSetManager::descriptorOffset(uint32_t setIdx, uint32_t bindingIdx) const
{
    int  offset     = 0;
    bool gotSet     = false;
    bool gotBinding = false;
    // bindInfo with same setIdx and bindingIdx would be merge to one bindingInfo
    std::for_each(_bindingInfos.begin(), _bindingInfos.end(),
                  [&](const BindInfo& bindInfo)
                  {
                      if (bindInfo.setIdx == setIdx)
                      {
                          gotSet = true;
                          if (bindInfo.bindingIdx == bindingIdx) gotBinding = true;
                      }
                      if (bindInfo.setIdx == setIdx && bindInfo.bindingIdx < bindingIdx)
                      {
                          offset += bindInfo.descriptorCount;
                      }
                  });
    assert(gotSet && gotBinding);
    return offset; // 或者其他适当的错误值
}

uint64_t DescriptorSetManager::getBindingsHash(uint32_t setIdx)
{
    assert(setIdx >= 3);
    if (_dirtyFlags)
    {
        for (int i = 0; i < _setBindingHashs.size(); ++i)
        {
            _setBindingHashs[i] = memoryHash(_descInfos[i]);
        }
        _dirtyFlags = 0;
    }
    return _setBindingHashs[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)];
}

uint64_t DescriptorSetManager::getDescsetLayoutHash(uint32_t setIdx)
{
    return memoryHash(_descBindSet[setIdx].getBindings());
}

std::vector<DescriptorSetManager::DescriptorInfo> DescriptorSetManager::getDescriptorInfo(uint32_t setIdx)
{
    assert(setIdx >= 3);

    return _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)];
}

DescriptorSetManager::DescriptorSetManager(const DescriptorSetManager&) {}

// DescriptorSetManager& DescriptorSetManager::operator=(const DescriptorSetManager&) {}

RenderProgram& RenderProgram::setVertexModuleID(ShaderID vertexModuleID)
{
    _vertexModuleID = vertexModuleID;
    return *this;
}

RenderProgram& RenderProgram::setFragModuleID(ShaderID fragModuleID)
{
    _fragModuleID = fragModuleID;
    return *this;
}

void RenderProgram::bind(VkCommandBuffer cmdBuf)
{
    VkPipeline gfxPipeline = getOrCreatePipeline(_renderPass);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
    VkDescriptorSet set =
        vkDriver->getDescriptorSetCache()->requestDescriptorSet(this->getDescriptorSetManager(), (uint32_t) DescriptorEnum::eDrawObjectDescriptorSet);
    VkBindDescriptorSetsInfo bindInfo{VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO};
    bindInfo.descriptorSetCount = 1;
    bindInfo.pDescriptorSets    = &set;
    bindInfo.stageFlags         = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    bindInfo.layout             = this->_descriptorSetManager.getPipelineLayout();
    bindInfo.firstSet           = static_cast<uint32_t>(DescriptorEnum::eDrawObjectDescriptorSet);
    vkCmdBindDescriptorSets2(cmdBuf, &bindInfo);
}

VkPipeline RenderProgram::getOrCreatePipeline(RenderPass* renderPass)
{
    return vkDriver->_pipelineCacheManager->getOrCreateGraphicsPipeline(this);
}

ComputeProgram& ComputeProgram::setComputeModuleID(ShaderID computeModuleID)
{
    _computeModuleID = computeModuleID;
    return *this;
}

void ComputeProgram::bind(VkCommandBuffer cmdBuf) {}

VkPipeline ComputeProgram::getOrCreatePipeline()
{
    return VK_NULL_HANDLE;
}

RTProgram& RTProgram::setRayGenModuleID(ShaderID rayGenModuleID)
{
    _rayGenModuleID = rayGenModuleID;
    return *this;
}

RTProgram& RTProgram::setRayCHitModuleID(ShaderID rayCHitModuleID)
{
    _rayCHitModuleID = rayCHitModuleID;
    return *this;
}

RTProgram& RTProgram::setRayAHitModuleID(ShaderID rayAHitModuleID)
{
    _rayAHitModuleID = rayAHitModuleID;
    return *this;
}

RTProgram& RTProgram::setRayMissModuleID(ShaderID rayMissModuleID)
{
    _rayMissModuleID = rayMissModuleID;
    return *this;
}

RTProgram& RTProgram::setRayIntersectModuleID(ShaderID rayIntersectModuleID)
{
    _rayIntersectModuleID = rayIntersectModuleID;
    return *this;
}

void RTProgram::bind(VkCommandBuffer cmdBuf) {}

VkPipeline RTProgram::getOrCreatePipeline()
{
    return VK_NULL_HANDLE;
}

MeshRenderProgram& MeshRenderProgram::setTaskModuleID(ShaderID taskModuleID)
{
    _taskModuleID = taskModuleID;
    return *this;
}

MeshRenderProgram& MeshRenderProgram::setMeshModuleID(ShaderID meshModuleID)
{
    _meshModuleID = meshModuleID;
    return *this;
}

MeshRenderProgram& MeshRenderProgram::setFragModuleID(ShaderID fragModuleID)
{
    _fragModuleID = fragModuleID;
    return *this;
}

void MeshRenderProgram::bind(VkCommandBuffer cmdBuf) {}

VkPipeline MeshRenderProgram::getOrCreatePipeline()
{
    return VK_NULL_HANDLE;
}
} // namespace Play