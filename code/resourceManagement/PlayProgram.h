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
    void                  deinit();
    DescriptorSetManager& addBinding(uint32_t setIdx, uint32_t bindingIdx, uint32_t descriptorCount,
                                     VkDescriptorType      descriptorType,
                                     VkPipelineStageFlags2 pipelineStageFlags);
    DescriptorSetManager& addBinding(const BindInfo& bindingInfo);
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
    DescriptorSetManager& initLayout();
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
    PlayProgram(VkDevice device) : _descriptorManager(device) {}
    virtual ~PlayProgram() {}
    virtual void deinit()
    {
        _descriptorManager.deinit();
    }
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
    RenderProgram(VkDevice device) : PlayProgram(device) {}
    ~RenderProgram() override {}
    RenderProgram(VkDevice device, ShaderID vertexModuleID, ShaderID fragModuleID)
        : PlayProgram(device), _vertexModuleID(vertexModuleID), _fragModuleID(fragModuleID)
    {
        finish();
    }
    RenderProgram&      setVertexModuleID(ShaderID vertexModuleID);
    RenderProgram&      setFragModuleID(ShaderID fragModuleID);
    void                finish();
    VkPipeline          getOrCreatePipeline() override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    ShaderID    _vertexModuleID = ~0U;
    ShaderID    _fragModuleID   = ~0U;
    ProgramType _programType    = ProgramType::eRenderProgram;
};

class ComputeProgram : public PlayProgram
{
public:
    ComputeProgram(VkDevice device) : PlayProgram(device) {}
    ComputeProgram(VkDevice device, ShaderID computeModuleID)
        : PlayProgram(device), _computeModuleID(computeModuleID)
    {
    }
    ComputeProgram&     setComputeModuleID(ShaderID computeModuleID);
    void                finish();
    VkPipeline          getOrCreatePipeline() override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    ShaderID    _computeModuleID = ~0U;
    ProgramType _programType     = ProgramType::eComputeProgram;
};

class RTProgram : public PlayProgram
{
public:
    RTProgram(VkDevice device) : PlayProgram(device) {}
    RTProgram(VkDevice device, ShaderID rayGenID, ShaderID rayCHitID, ShaderID rayMissID)
        : PlayProgram(device),
          _rayGenModuleID(rayGenID),
          _rayCHitModuleID(rayCHitID),
          _rayMissModuleID(rayMissID)
    {
    }
    RTProgram&          setRayGenModuleID(ShaderID rayGenModuleID);
    RTProgram&          setRayCHitModuleID(ShaderID rayCHitModuleID);
    RTProgram&          setRayAHitModuleID(ShaderID rayAHitModuleID);
    RTProgram&          setRayMissModuleID(ShaderID rayMissModuleID);
    RTProgram&          setRayIntersectModuleID(ShaderID rayIntersectModuleID);
    void                finish();
    VkPipeline          getOrCreatePipeline() override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    ShaderID    _rayGenModuleID       = ~0U;
    ShaderID    _rayCHitModuleID      = ~0U;
    ShaderID    _rayAHitModuleID      = ~0U;
    ShaderID    _rayMissModuleID      = ~0U;
    ShaderID    _rayIntersectModuleID = ~0U;
    ProgramType _programType          = ProgramType::eRTProgram;
};

class MeshRenderProgram : public PlayProgram
{
public:
    MeshRenderProgram(VkDevice device) : PlayProgram(device) {}
    MeshRenderProgram(VkDevice device, ShaderID meshModuleID, ShaderID fragModuleID)
        : PlayProgram(device), _meshModuleID(meshModuleID), _fragModuleID(fragModuleID)
    {
    }
    MeshRenderProgram&  setTaskModuleID(ShaderID taskModuleID);
    MeshRenderProgram&  setMeshModuleID(ShaderID meshModuleID);
    MeshRenderProgram&  setFragModuleID(ShaderID fragModuleID);
    void                finish();
    VkPipeline          getOrCreatePipeline() override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    ShaderID    _taskModuleID = ~0U;
    ShaderID    _meshModuleID = ~0U;
    ShaderID    _fragModuleID = ~0U;
    ProgramType _programType  = ProgramType::eMeshRenderProgram;
};
} // namespace Play

#endif // PLAYPROGRAM_H