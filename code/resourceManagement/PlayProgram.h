#ifndef PLAYPROGRAM_H
#define PLAYPROGRAM_H

#include "PlayApp.h"
#include <nvvk/descriptors.hpp>
#include <ShaderManager.hpp>

namespace Play
{
using ShaderID = uint32_t;

struct BindInfo
{
    uint32_t              setIdx;
    uint32_t              bindingIdx;
    uint32_t              descriptorCount;
    VkDescriptorType      descriptorType;
    VkPipelineStageFlags2 pipelineStageFlags;
};

struct ConstantRange
{
    size_t                size;
    VkPipelineStageFlags2 stage;
};
/*
Example:


*/

class DescriptorSetManager : public nvvk::WriteSetContainer
{
public:
    DescriptorSetManager(VkDevice device);
    ~DescriptorSetManager();
    DescriptorSetManager& addBinding(uint32_t setIdx, uint32_t bindingIdx, uint32_t descriptorCount,
                                     VkDescriptorType      descriptorType,
                                     VkPipelineStageFlags2 pipelineStageFlags);
    DescriptorSetManager& addBinding(const BindInfo& bindingInfo);
    DescriptorSetManager& initLayout();
    bool                  finish();
    DescriptorSetManager& addConstantRange(uint32_t size, uint32_t offset,
                                           VkPipelineStageFlags2 stage);

    VkPipelineLayout                                                  getPipelineLayout() const;
    std::array<nvvk::DescriptorBindings, MAX_DESCRIPTOR_SETS::value>& getSetBindingInfo()
    {
        return _descBindSet;
    }

    std::array<VkDescriptorSetLayout, MAX_DESCRIPTOR_SETS::value>& getDescriptorSetLayouts()
    {
        return _descSetLayouts;
    }

private:
    DescriptorSetManager(const DescriptorSetManager&);
    DescriptorSetManager& operator=(const DescriptorSetManager&);

    VkDevice getDevice() const
    {
        return _vkDevice;
    }
    bool _recordState = false; // 记录状态,保证binding有增改之后会调用finish,刷新管线布局
    std::vector<BindInfo>                                            _bindingInfos;
    std::vector<VkPushConstantRange>                                 _constantRanges;
    std::array<nvvk::DescriptorBindings, MAX_DESCRIPTOR_SETS::value> _descBindSet;
    std::array<VkDescriptorSetLayout, MAX_DESCRIPTOR_SETS::value>    _descSetLayouts = {};
    VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
    VkDevice         _vkDevice;
    // Additional functionality for managing descriptor sets can be added here
};

enum class ProgramType
{
    eRenderProgram,
    eComputeProgram,
    eRTProgram,
    eMeshRenderProgram,
    eUndefined
};

class PlayProgram
{
public:
    PlayProgram();
    ~PlayProgram();
    PlayProgram(const PlayProgram&);
    PlayProgram&          operator=(const PlayProgram&);
    virtual VkPipeline    getOrCreatePipeline()  = 0;
    virtual ProgramType   getProgramType() const = 0;
    DescriptorSetManager& getDescriptorManager()
    {
        return _descriptorManager;
    }
    VkPipelineBindPoint getPipelineBindPoint() const
    {
        if (_programType == ProgramType::eRenderProgram ||
            _programType == ProgramType::eMeshRenderProgram)
        {
            return VK_PIPELINE_BIND_POINT_GRAPHICS;
        }
        else if (_programType == ProgramType::eComputeProgram)
        {
            return VK_PIPELINE_BIND_POINT_COMPUTE;
        }
        else if (_programType == ProgramType::eRTProgram)
        {
            return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        }
        LOGW("Undefined program type, return VK_PIPELINE_BIND_POINT_MAX_ENUM\n");
        return VK_PIPELINE_BIND_POINT_MAX_ENUM;
    }

protected:
    DescriptorSetManager _descriptorManager;
    ProgramType          _programType = ProgramType::eUndefined;
};

class RenderProgram : public PlayProgram
{
public:
    RenderProgram() = default;
    RenderProgram(ShaderID vertexModuleID, ShaderID fragModuleID)
        : _vertexModuleID(vertexModuleID), _fragModuleID(fragModuleID)
    {
    }
    void setVertexModuleID(ShaderID vertexModuleID)
    {
        _vertexModuleID = vertexModuleID;
    }
    void setFragModuleID(ShaderID fragModuleID)
    {
        _fragModuleID = fragModuleID;
    }
    VkPipeline          getOrCreatePipeline() override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    ShaderID    _vertexModuleID;
    ShaderID    _fragModuleID;
    ProgramType _programType = ProgramType::eRenderProgram;
};

class ComputeProgram : public PlayProgram
{
public:
    ComputeProgram() = default;
    ComputeProgram(ShaderID computeModuleID) : _computeModuleID(computeModuleID) {}
    void setComputeModuleID(ShaderID computeModuleID)
    {
        _computeModuleID = computeModuleID;
    }
    VkPipeline          getOrCreatePipeline() override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    ShaderID    _computeModuleID;
    ProgramType _programType = ProgramType::eComputeProgram;
};

class RTProgram : public PlayProgram
{
public:
    RTProgram() = default;
    RTProgram(ShaderID rayGenID, ShaderID rayCHitID, ShaderID rayMissID)
        : _rayGenModuleID(rayGenID), _rayCHitModuleID(rayCHitID), _rayMissModuleID(rayMissID)
    {
    }
    void setRayGenModuleID(ShaderID rayGenModuleID)
    {
        _rayGenModuleID = rayGenModuleID;
    }
    void setRayCHitModuleID(ShaderID rayCHitModuleID)
    {
        _rayCHitModuleID = rayCHitModuleID;
    }
    void setRayAHitModuleID(ShaderID rayAHitModuleID)
    {
        _rayAHitModuleID = rayAHitModuleID;
    }
    void setRayMissModuleID(ShaderID rayMissModuleID)
    {
        _rayMissModuleID = rayMissModuleID;
    }
    void setRayIntersectModuleID(ShaderID rayIntersectModuleID)
    {
        _rayIntersectModuleID = rayIntersectModuleID;
    }
    VkPipeline          getOrCreatePipeline() override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    ShaderID    _rayGenModuleID;
    ShaderID    _rayCHitModuleID;
    ShaderID    _rayAHitModuleID;
    ShaderID    _rayMissModuleID;
    ShaderID    _rayIntersectModuleID;
    ProgramType _programType = ProgramType::eRTProgram;
};

class MeshRenderProgram : public PlayProgram
{
public:
    MeshRenderProgram() = default;
    MeshRenderProgram(ShaderID meshModuleID, ShaderID fragModuleID)
        : _meshModuleID(meshModuleID), _fragModuleID(fragModuleID)
    {
    }
    void setTaskModuleID(ShaderID taskModuleID)
    {
        _taskModuleID = taskModuleID;
    }
    void setMeshModuleID(ShaderID meshModuleID)
    {
        _meshModuleID = meshModuleID;
    }
    void setFragModuleID(ShaderID fragModuleID)
    {
        _fragModuleID = fragModuleID;
    }
    VkPipeline          getOrCreatePipeline() override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    ShaderID    _taskModuleID;
    ShaderID    _meshModuleID;
    ShaderID    _fragModuleID;
    ProgramType _programType = ProgramType::eMeshRenderProgram;
};
} // namespace Play

#endif // PLAYPROGRAM_H