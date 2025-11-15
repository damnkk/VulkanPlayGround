
#include "PlayProgram.h"
#include "nvvk/check_error.hpp"
#include "spirv_reflect.h"
#include "ShaderManager.hpp"
#include "crc32c/crc32c.h"
namespace Play
{
DescriptorSetManager::DescriptorSetManager(VkDevice device) : _vkDevice(device) {}

DescriptorSetManager::~DescriptorSetManager() {}
void DescriptorSetManager::deinit()
{
    if (_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(_vkDevice, _pipelineLayout, nullptr);
        _pipelineLayout = VK_NULL_HANDLE;
    }
    for (auto& layout : _descSetLayouts)
    {
        if (layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(_vkDevice, layout, nullptr);
            layout = VK_NULL_HANDLE;
        }
    }
}
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
            _bindingInfos[i].pipelineStageFlags |= bindingInfo.pipelineStageFlags;
            _bindingInfos[i].descriptorCount += bindingInfo.descriptorCount;
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
    for (int i = 0; i < uint32_t(DescriptorEnum::eCount); ++i)
    {
        _descBindSet[i].createDescriptorSetLayout(
            _vkDevice, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
            &_descSetLayouts[i]);
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
    for (const auto& bindings : _bindingInfos)
    {
        assert(bindings.setIdx <= uint32_t(DescriptorEnum::eCount));
        auto& set = _descBindSet[bindings.setIdx];
        set.addBinding(bindings.bindingIdx, bindings.descriptorType, bindings.descriptorCount,
                       bindings.pipelineStageFlags);
    }
    initLayout();
    uint32_t descriptorCount = 0;
    std::for_each(_bindingInfos.begin(), _bindingInfos.end(),
                  [&](const BindInfo& info)
                  {
                      if (info.setIdx == uint32_t(DescriptorEnum::ePerPassDescriptorSet) - 3)
                          descriptorCount += info.descriptorCount;
                  });
    _descInfos[0].resize(descriptorCount);
    descriptorCount = 0;
    std::for_each(_bindingInfos.begin(), _bindingInfos.end(),
                  [&](const BindInfo& info)
                  {
                      if (info.setIdx == uint32_t(DescriptorEnum::eDrawObjectDescriptorSet) - 3)
                          descriptorCount += info.descriptorCount;
                  });
    _descInfos[1].resize(descriptorCount);
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

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const nvvk::Buffer& buffer, VkDeviceSize offset,
                                       VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                 [descriptorOffset(setIdx, bindingIdx)]
                                     .buffer;
    if (bufferInfo.buffer == buffer.buffer && bufferInfo.offset == offset &&
        bufferInfo.range == range)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    bufferInfo.buffer = buffer.buffer;
    bufferInfo.offset = offset;
    bufferInfo.range  = range;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const nvvk::AccelerationStructure& accel)
{
    auto& accelInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                [descriptorOffset(setIdx, bindingIdx)]
                                    .accel;
    if (accelInfo == accel.accel)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    accelInfo = accel.accel;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const nvvk::Image& image)
{
    auto& imageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                [descriptorOffset(setIdx, bindingIdx)]
                                    .image;
    if (imageInfo.imageLayout == image.descriptor.imageLayout &&
        imageInfo.imageView == image.descriptor.imageView &&
        imageInfo.sampler == image.descriptor.sampler)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    imageInfo = image.descriptor;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, VkBuffer buffer,
                                       VkDeviceSize offset, VkDeviceSize range)
{
    auto& bufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                 [descriptorOffset(setIdx, bindingIdx)]
                                     .buffer;
    if (bufferInfo.buffer == buffer && bufferInfo.offset == offset && bufferInfo.range == range)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range  = range;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const VkDescriptorBufferInfo& bufferInfo)
{
    auto& destBufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                     [descriptorOffset(setIdx, bindingIdx)]
                                         .buffer;
    if (destBufferInfo.buffer == bufferInfo.buffer && destBufferInfo.offset == bufferInfo.offset &&
        destBufferInfo.range == bufferInfo.range)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    destBufferInfo = bufferInfo;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx, VkImageView imageView,
                                       VkImageLayout imageLayout, VkSampler sampler)
{
    auto& imageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                [descriptorOffset(setIdx, bindingIdx)]
                                    .image;
    if (imageInfo.imageLayout == imageLayout && imageInfo.imageView == imageView &&
        imageInfo.sampler == sampler)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    imageInfo.imageLayout = imageLayout;
    imageInfo.imageView   = imageView;
    imageInfo.sampler     = sampler;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const VkDescriptorImageInfo& imageInfo)
{
    auto& destImageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                    [descriptorOffset(setIdx, bindingIdx)]
                                        .image;
    if (destImageInfo.imageLayout == imageInfo.imageLayout &&
        destImageInfo.imageView == imageInfo.imageView &&
        destImageInfo.sampler == imageInfo.sampler)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    destImageInfo = imageInfo;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       VkAccelerationStructureKHR accel)
{
    auto& accelInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                [descriptorOffset(setIdx, bindingIdx)]
                                    .accel;
    if (accelInfo == accel)
    {
        return;
    }
    _dirtyFlags |= 1 << 0;
    accelInfo = accel;
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const nvvk::Buffer* buffers, uint32_t count)
{
    auto& bufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                 [descriptorOffset(setIdx, bindingIdx)]
                                     .buffer;
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

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const nvvk::AccelerationStructure* accels, uint32_t count)
{
    auto& accelInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                [descriptorOffset(setIdx, bindingIdx)]
                                    .accel;
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

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const nvvk::Image* images, uint32_t count)
{
    auto& imageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                [descriptorOffset(setIdx, bindingIdx)]
                                    .image;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (imageInfo.imageLayout == images[i].descriptor.imageLayout &&
            imageInfo.imageView == images[i].descriptor.imageView &&
            imageInfo.sampler == images[i].descriptor.sampler)
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        imageInfo = images[i].descriptor;
    }
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const VkDescriptorBufferInfo* bufferInfos, uint32_t count)
{
    auto& destBufferInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                     [descriptorOffset(setIdx, bindingIdx)]
                                         .buffer;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (destBufferInfo.buffer == bufferInfos[i].buffer &&
            destBufferInfo.offset == bufferInfos[i].offset &&
            destBufferInfo.range == bufferInfos[i].range)
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        destBufferInfo = bufferInfos[i];
    }
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const VkDescriptorImageInfo* imageInfos, uint32_t count)
{
    auto& imageInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                [descriptorOffset(setIdx, bindingIdx)]
                                    .image;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (imageInfo.imageLayout == imageInfos[i].imageLayout &&
            imageInfo.imageView == imageInfos[i].imageView &&
            imageInfo.sampler == imageInfos[i].sampler)
        {
            continue;
        }
        _dirtyFlags |= 1 << 0;
        imageInfo = imageInfos[i];
    }
}

void DescriptorSetManager::setDescInfo(uint32_t setIdx, uint32_t bindingIdx,
                                       const VkAccelerationStructureKHR* accels, uint32_t count)
{
    auto& accelInfo = _descInfos[setIdx - uint32_t(DescriptorEnum::ePerPassDescriptorSet)]
                                [descriptorOffset(setIdx, bindingIdx)]
                                    .accel;
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
    std::for_each(_bindingInfos.begin(), _bindingInfos.end(),
                  [&](const BindInfo& bindInfo)
                  {
                      if (bindInfo.setIdx == setIdx)
                      {
                          gotSet = true;
                          if (bindInfo.bindingIdx == bindingIdx) gotBinding = true;
                          if (bindInfo.bindingIdx < bindingIdx)
                          {
                              offset += bindInfo.descriptorCount;
                          }
                      }
                  });
    assert(gotSet && gotBinding && offset);
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

std::vector<DescriptorSetManager::DescriptorInfo> DescriptorSetManager::getDescriptorInfo(
    uint32_t setIdx)
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

void RenderProgram::finish()
{
    const ShaderModule* vertModule = ShaderManager::Instance().getShaderById(_vertexModuleID);
    const ShaderModule* fragModule = ShaderManager::Instance().getShaderById(_fragModuleID);
    // … inside RenderProgram::finish(), after拿到vert/frag module
    const ShaderModule* modules[] = {vertModule, fragModule};
    for (auto* module : modules)
    {
        SpvReflectShaderModule spvModule{};
        SpvReflectResult       result = spvReflectCreateShaderModule(module->_spvCode.size(),
                                                                     module->_spvCode.data(), &spvModule);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        { /* error */
            continue;
        }

        uint32_t setCount = 0;
        spvReflectEnumerateDescriptorSets(&spvModule, &setCount, nullptr);
        std::vector<SpvReflectDescriptorSet*> sets(setCount);
        spvReflectEnumerateDescriptorSets(&spvModule, &setCount, sets.data());
        std::vector<BindInfo> bindInfos;
        for (SpvReflectDescriptorSet* set : sets)
        {
            for (uint32_t i = 0; i < set->binding_count; ++i)
            {
                const SpvReflectDescriptorBinding& reflBinding = *set->bindings[i];
                BindInfo                           info{};
                info.setIdx             = set->set;
                info.bindingIdx         = reflBinding.binding;
                info.descriptorCount    = reflBinding.count;
                info.descriptorType     = spvToDescriptorType(reflBinding.descriptor_type);
                info.pipelineStageFlags = spvToVkStageFlags(spvModule.shader_stage);
                bindInfos.push_back(info);
            }
        }

        for (const auto& bindInfo : bindInfos)
        {
            _descriptorSetManager.addBinding(bindInfo);
        }

        uint32_t pushConstCount = 0;
        spvReflectEnumeratePushConstantBlocks(&spvModule, &pushConstCount, nullptr);
        std::vector<SpvReflectBlockVariable*> pushConsts(pushConstCount);
        spvReflectEnumeratePushConstantBlocks(&spvModule, &pushConstCount, pushConsts.data());
        for (const auto& pushConst : pushConsts)
        {
            VkPushConstantRange range{};
            range.offset = pushConst->offset;
            range.size   = pushConst->size;
            range.stageFlags |= spvToVkStageFlags(spvModule.shader_stage);
            _descriptorSetManager.addConstantRange(range.size, range.offset, range.stageFlags);
        }

        spvReflectDestroyShaderModule(&spvModule);
    }
    _descriptorSetManager.finish();
}

VkPipeline RenderProgram::getOrCreatePipeline()
{
    return VK_NULL_HANDLE;
}

ComputeProgram& ComputeProgram::setComputeModuleID(ShaderID computeModuleID)
{
    _computeModuleID = computeModuleID;
    return *this;
}

void ComputeProgram::finish()
{
    const ShaderModule* compModule = ShaderManager::Instance().getShaderById(_computeModuleID);
    std::vector<const ShaderModule*> modules = {compModule};
    for (auto* module : modules)
    {
        SpvReflectShaderModule spvModule{};
        SpvReflectResult       result = spvReflectCreateShaderModule(module->_spvCode.size(),
                                                                     module->_spvCode.data(), &spvModule);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        { /* error */
            continue;
        }

        uint32_t setCount = 0;
        spvReflectEnumerateDescriptorSets(&spvModule, &setCount, nullptr);
        std::vector<SpvReflectDescriptorSet*> sets(setCount);
        spvReflectEnumerateDescriptorSets(&spvModule, &setCount, sets.data());
        std::vector<BindInfo> bindInfos;
        for (SpvReflectDescriptorSet* set : sets)
        {
            for (uint32_t i = 0; i < set->binding_count; ++i)
            {
                const SpvReflectDescriptorBinding& reflBinding = *set->bindings[i];
                BindInfo                           info{};
                info.setIdx             = set->set;
                info.bindingIdx         = reflBinding.binding;
                info.descriptorCount    = reflBinding.count;
                info.descriptorType     = spvToDescriptorType(reflBinding.descriptor_type);
                info.pipelineStageFlags = spvToVkStageFlags(spvModule.shader_stage);
                bindInfos.push_back(info);
            }
        }

        for (const auto& bindInfo : bindInfos)
        {
            _descriptorSetManager.addBinding(bindInfo);
        }

        uint32_t pushConstCount = 0;
        spvReflectEnumeratePushConstantBlocks(&spvModule, &pushConstCount, nullptr);
        std::vector<SpvReflectBlockVariable*> pushConsts(pushConstCount);
        spvReflectEnumeratePushConstantBlocks(&spvModule, &pushConstCount, pushConsts.data());
        for (const auto& pushConst : pushConsts)
        {
            VkPushConstantRange range{};
            range.offset = pushConst->offset;
            range.size   = pushConst->size;
            range.stageFlags |= spvToVkStageFlags(spvModule.shader_stage);
            _descriptorSetManager.addConstantRange(range.size, range.offset, range.stageFlags);
        }
        _descriptorSetManager.finish();

        spvReflectDestroyShaderModule(&spvModule);
    }
}

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

void RTProgram::finish()
{
    const ShaderModule* rayGenModule  = ShaderManager::Instance().getShaderById(_rayGenModuleID);
    const ShaderModule* rayCHitModule = ShaderManager::Instance().getShaderById(_rayCHitModuleID);
    const ShaderModule* rayAHitModule = ShaderManager::Instance().getShaderById(_rayAHitModuleID);
    const ShaderModule* rayMissModule = ShaderManager::Instance().getShaderById(_rayMissModuleID);
    const ShaderModule* rayIntersectModule =
        ShaderManager::Instance().getShaderById(_rayIntersectModuleID);
    std::vector<const ShaderModule*> modules = {rayGenModule, rayCHitModule, rayAHitModule,
                                                rayMissModule, rayIntersectModule};
    for (auto* module : modules)
    {
        if (!module) continue;
        SpvReflectShaderModule spvModule{};
        SpvReflectResult       result = spvReflectCreateShaderModule(module->_spvCode.size(),
                                                                     module->_spvCode.data(), &spvModule);
        if (result != SPV_REFLECT_RESULT_SUCCESS)
        { /* error */
            continue;
        }

        uint32_t setCount = 0;
        spvReflectEnumerateDescriptorSets(&spvModule, &setCount, nullptr);
        std::vector<SpvReflectDescriptorSet*> sets(setCount);
        spvReflectEnumerateDescriptorSets(&spvModule, &setCount, sets.data());
        std::vector<BindInfo> bindInfos;
        for (SpvReflectDescriptorSet* set : sets)
        {
            for (uint32_t i = 0; i < set->binding_count; ++i)
            {
                const SpvReflectDescriptorBinding& reflBinding = *set->bindings[i];
                BindInfo                           info{};
                info.setIdx             = set->set;
                info.bindingIdx         = reflBinding.binding;
                info.descriptorCount    = reflBinding.count;
                info.descriptorType     = spvToDescriptorType(reflBinding.descriptor_type);
                info.pipelineStageFlags = spvToVkStageFlags(spvModule.shader_stage);
                bindInfos.push_back(info);
            }
        }

        for (const auto& bindInfo : bindInfos)
        {
            _descriptorSetManager.addBinding(bindInfo);
        }

        uint32_t pushConstCount = 0;
        spvReflectEnumeratePushConstantBlocks(&spvModule, &pushConstCount, nullptr);
        std::vector<SpvReflectBlockVariable*> pushConsts(pushConstCount);
        spvReflectEnumeratePushConstantBlocks(&spvModule, &pushConstCount, pushConsts.data());
        for (const auto& pushConst : pushConsts)
        {
            VkPushConstantRange range{};
            range.offset = pushConst->offset;
            range.size   = pushConst->size;
            range.stageFlags |= spvToVkStageFlags(spvModule.shader_stage);
            _descriptorSetManager.addConstantRange(range.size, range.offset, range.stageFlags);
        }
        _descriptorSetManager.finish();

        spvReflectDestroyShaderModule(&spvModule);
    }
}

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

void MeshRenderProgram::finish() {}

VkPipeline MeshRenderProgram::getOrCreatePipeline()
{
    return VK_NULL_HANDLE;
}
} // namespace Play