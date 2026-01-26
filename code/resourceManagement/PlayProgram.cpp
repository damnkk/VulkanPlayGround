
#include "PlayProgram.h"
#include "nvvk/check_error.hpp"
#include "spirv_reflect.h"
#include "ShaderManager.hpp"
#include "crc32c/crc32c.h"
#include "VulkanDriver.h"
#include "RDG/RDGPasses.hpp"
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

void PushConstantManager::pushConstantRanges(VkCommandBuffer cmdBuf, VkPipelineLayout layout)
{
    VkPushConstantsInfo pushConstantsInfo{VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO};
    pushConstantsInfo.layout = layout;
    for (const auto& range : _ranges)
    {
        pushConstantsInfo.offset     = range.offset;
        pushConstantsInfo.size       = range.size;
        pushConstantsInfo.stageFlags = range.stageFlags;
        pushConstantsInfo.pValues    = _constantData.data();
        vkCmdPushConstants(cmdBuf, layout, range.stageFlags, range.offset, range.size, _constantData.data());
    }
}

DescriptorSetManager::DescriptorSetManager() {}

DescriptorSetManager::~DescriptorSetManager()
{
    if (_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(vkDriver->getDevice(), _pipelineLayout, nullptr);
        _pipelineLayout = VK_NULL_HANDLE;
    }
    for (size_t i = (size_t) DescriptorEnum::ePerPassDescriptorSet; i < (size_t) DescriptorEnum::eCount; ++i)
    {
        auto& layout = _descSetLayouts[i];
        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(vkDriver->getDevice(), layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }
}

DescriptorSetBindings::DescriptorSetBindings() {}
DescriptorSetBindings::~DescriptorSetBindings() {}
DescriptorSetBindings& DescriptorSetBindings::addBinding(const BindInfo& bindingInfo)
{
    _setLayoutDirty = true;
    for (int i = 0; i < _bindingInfos.size(); i++)
    {
        if (_bindingInfos[i].bindingIdx == bindingInfo.bindingIdx)
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

DescriptorSetBindings& DescriptorSetBindings::addBinding(uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType,
                                                         VkShaderStageFlags shaderStageFlags)
{
    BindInfo bindingInfo = {bindingIdx, descriptorCount, descriptorType, shaderStageFlags};
    return addBinding(bindingInfo);
}

void DescriptorSetManager::finalizePipelineLayoutImpl()
{
    if (_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vkDriver->getDevice(), _pipelineLayout, nullptr);

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCreateInfo.setLayoutCount         = static_cast<uint32_t>(_descSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts            = _descSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(_constantRanges.getRanges().size());
    pipelineLayoutCreateInfo.pPushConstantRanges    = _constantRanges.getRanges().data();
    NVVK_CHECK(vkCreatePipelineLayout(vkDriver->getDevice(), &pipelineLayoutCreateInfo, nullptr, &_pipelineLayout));

    return;
}

void DescriptorSetManager::finalizePipelineLayout()
{
    if (!_pipelineLayoutDirty) return; // if no addBinding(s) called after last layout finalization, just return
    // prepare desc set layouts
    _descSetLayouts[uint32_t(DescriptorEnum::eGlobalDescriptorSet)] = vkDriver->getDescriptorSetCache()->getEngineDescriptorSet().layout;
    _descSetLayouts[uint32_t(DescriptorEnum::eSceneDescriptorSet)]  = vkDriver->getDescriptorSetCache()->getSceneDescriptorSet().layout;
    _descSetLayouts[uint32_t(DescriptorEnum::eFrameDescriptorSet)]  = vkDriver->getDescriptorSetCache()->getFrameDescriptorSet().layout;

    for (const auto& bindings : _bindingInfos)
    {
        nvvk::DescriptorBindings::addBinding(bindings.bindingIdx, bindings.descriptorType, bindings.descriptorCount, bindings.shaderStageFlags);
    }
    _descSetLayouts[(uint32_t) DescriptorEnum::eDrawObjectDescriptorSet] = finalizeLayout(); // finalize curr set layout
    finalizePipelineLayoutImpl();
    _pipelineLayoutDirty = 0;
    return;
}

VkPipelineLayout DescriptorSetManager::getPipelineLayout() const
{
    if (_pipelineLayoutDirty)
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

VkDescriptorSetLayout DescriptorSetBindings::finalizeLayout()
{
    if (!_setLayoutDirty) return _layout;
    if (_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vkDriver->getDevice(), _layout, nullptr);
        _layout = VK_NULL_HANDLE;
    }
    uint32_t descriptorCount = 0;
    for (const auto& binding : _bindingInfos)
    {
        descriptorCount += binding.descriptorCount;
        nvvk::DescriptorBindings::addBinding(binding.bindingIdx, binding.descriptorType, binding.descriptorCount, binding.shaderStageFlags);
    }
    _descInfos.resize(descriptorCount);
    createDescriptorSetLayout(vkDriver->getDevice(), 0, &_layout);
    _setLayoutDirty = false;
    return _layout;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Buffer& buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx)].buffer;
    if (bufferInfo.buffer == buffer.buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    _descInfoDirty |= 1 << 0;
    bufferInfo.buffer = buffer.buffer;
    bufferInfo.offset = offset;
    bufferInfo.range  = range;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Image& image)
{
    auto& imageInfo = _descInfos[descriptorOffset(bindingIdx)];
    if (imageInfo.image.imageLayout == image.descriptor.imageLayout && imageInfo.image.imageView == image.descriptor.imageView &&
        imageInfo.image.sampler == image.descriptor.sampler)
    {
        return;
    }
    _descInfoDirty |= 1 << 0;
    imageInfo.image = image.descriptor;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx)].buffer;
    if (bufferInfo.buffer == buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    _descInfoDirty |= 1 << 0;
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range  = range;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkDescriptorBufferInfo& bufferInfo)
{
    auto& destBufferInfo = _descInfos[descriptorOffset(bindingIdx)].buffer;
    if (destBufferInfo.buffer == bufferInfo.buffer && destBufferInfo.offset == bufferInfo.offset && destBufferInfo.range == bufferInfo.range)
    {
        return;
    }
    _descInfoDirty |= 1 << 0;
    destBufferInfo = bufferInfo;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkImageView imageView, VkImageLayout imageLayout, VkSampler sampler)
{
    auto& imageInfo = _descInfos[descriptorOffset(bindingIdx)].image;
    if (imageInfo.imageLayout == imageLayout && imageInfo.imageView == imageView && imageInfo.sampler == sampler)
    {
        return;
    }
    _descInfoDirty |= 1 << 0;
    imageInfo.imageLayout = imageLayout;
    imageInfo.imageView   = imageView;
    imageInfo.sampler     = sampler;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkDescriptorImageInfo& imageInfo)
{
    auto& destImageInfo = _descInfos[descriptorOffset(bindingIdx)].image;
    if (destImageInfo.imageLayout == imageInfo.imageLayout && destImageInfo.imageView == imageInfo.imageView &&
        destImageInfo.sampler == imageInfo.sampler)
    {
        return;
    }
    _descInfoDirty |= 1 << 0;
    destImageInfo = imageInfo;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, VkAccelerationStructureKHR accel)
{
    auto& accelInfo = _descInfos[descriptorOffset(bindingIdx)].accel;
    if (accelInfo == accel)
    {
        return;
    }
    _descInfoDirty |= 1 << 0;
    accelInfo = accel;
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Buffer* buffers, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& bufferInfo = _descInfos[descriptorOffset(bindingIdx) + i].buffer;
        if (bufferInfo.buffer == buffers[i].buffer)
        {
            continue;
        }
        _descInfoDirty |= 1 << 0;
        bufferInfo.buffer = buffers[i].buffer;
    }
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const nvvk::Image* images, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& imageInfo = _descInfos[descriptorOffset(bindingIdx) + i].image;
        if (imageInfo.imageLayout == images[i].descriptor.imageLayout && imageInfo.imageView == images[i].descriptor.imageView &&
            imageInfo.sampler == images[i].descriptor.sampler)
        {
            continue;
        }
        _descInfoDirty |= 1 << 0;
        imageInfo = images[i].descriptor;
    }
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkDescriptorBufferInfo* bufferInfos, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& destBufferInfo = _descInfos[descriptorOffset(bindingIdx) + i].buffer;
        if (destBufferInfo.buffer == bufferInfos[i].buffer && destBufferInfo.offset == bufferInfos[i].offset &&
            destBufferInfo.range == bufferInfos[i].range)
        {
            continue;
        }
        _descInfoDirty |= 1 << 0;
        destBufferInfo = bufferInfos[i];
    }
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkDescriptorImageInfo* imageInfos, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& imageInfo = _descInfos[descriptorOffset(bindingIdx) + 1].image;
        if (imageInfo.imageLayout == imageInfos[i].imageLayout && imageInfo.imageView == imageInfos[i].imageView &&
            imageInfo.sampler == imageInfos[i].sampler)
        {
            continue;
        }
        _descInfoDirty |= 1 << 0;
        imageInfo = imageInfos[i];
    }
}

void DescriptorSetBindings::setDescInfo(uint32_t bindingIdx, const VkAccelerationStructureKHR* accels, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto& accelInfo = _descInfos[descriptorOffset(bindingIdx) + i].accel;
        if (accelInfo == accels[i])
        {
            continue;
        }
        _descInfoDirty |= 1 << 0;
        accelInfo = accels[i];
    }
}

int DescriptorSetBindings::descriptorOffset(uint32_t bindingIdx)
{
    int  offset     = 0;
    bool gotBinding = false;
    // bindInfo with same setIdx and bindingIdx would be merge to one bindingInfo
    std::for_each(_bindingInfos.begin(), _bindingInfos.end(),
                  [&](const BindInfo& bindInfo)
                  {
                      if (bindInfo.bindingIdx == bindingIdx) gotBinding = true;
                      if (bindInfo.bindingIdx < bindingIdx)
                      {
                          offset += bindInfo.descriptorCount;
                      }
                  });
    assert(gotBinding);
    return gotBinding ? offset : -1; // 或者其他适当的错误值
}

uint64_t DescriptorSetBindings::getBindingsHash()
{
    if (_descInfoDirty)
    {
        _setBindingHash = memoryHash(_descInfos);

        _descInfoDirty = 0;
    }
    return _setBindingHash;
}

uint64_t DescriptorSetBindings::getDescsetLayoutHash()
{
    return memoryHash(_bindingInfos);
}

DescriptorSetManager::DescriptorSetManager(const DescriptorSetManager&) {}

// DescriptorSetManager& DescriptorSetManager::operator=(const DescriptorSetManager&) {}

void RenderProgram::setPassNode(RDG::PassNode* passNode)
{
    _passNode = passNode;
    _descriptorSetManager.setDescriptorSetLayout(DescriptorEnum::ePerPassDescriptorSet, _passNode->getDescriptorBindings().getSetLayout());
    finish();
}

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
    VkPipeline gfxPipeline = getOrCreatePipeline();
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
}

RDG::RenderPassNode* RenderProgram::getPassNode()
{
    return static_cast<RDG::RenderPassNode*>(_passNode);
}

VkPipeline RenderProgram::getOrCreatePipeline()
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