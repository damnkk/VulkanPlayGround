
#include "PlayProgram.h"
#include "nvvk/check_error.hpp"
#include "spirv_reflect.h"
#include "ShaderManager.hpp"
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
    for (const auto& bindings : _bindingInfos)
    {
        assert(bindings.setIdx >= MAX_DESCRIPTOR_SETS::value);
        auto& set = _descBindSet[bindings.setIdx];
        set.addBinding(bindings.bindingIdx, bindings.descriptorType, bindings.descriptorCount,
                       bindings.pipelineStageFlags);
    }
    initLayout();
    for (auto& set : _descBindSet)
    {
        set.clear();
    }
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
            _descriptorManager.addBinding(bindInfo);
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
            _descriptorManager.addConstantRange(range.size, range.offset, range.stageFlags);
        }

        spvReflectDestroyShaderModule(&spvModule);
    }
    _descriptorManager.finish();
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
            _descriptorManager.addBinding(bindInfo);
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
            _descriptorManager.addConstantRange(range.size, range.offset, range.stageFlags);
        }
        _descriptorManager.finish();

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
            _descriptorManager.addBinding(bindInfo);
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
            _descriptorManager.addConstantRange(range.size, range.offset, range.stageFlags);
        }
        _descriptorManager.finish();

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