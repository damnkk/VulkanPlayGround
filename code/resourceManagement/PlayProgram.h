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

    VkPipelineLayout getPipelineLayout() const;

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
    VkDevice                                                         _vkDevice;
    // Additional functionality for managing descriptor sets can be added here
};

class PlayProgram
{
   public:
    PlayProgram();
    ~PlayProgram();
    PlayProgram(const PlayProgram&);
    PlayProgram&       operator=(const PlayProgram&);
    virtual VkPipeline getOrCreatePipeline() = 0;

   protected:
    DescriptorSetManager _descriptorManager;
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
    VkPipeline getOrCreatePipeline() override;

   private:
    ShaderID _vertexModuleID;
    ShaderID _fragModuleID;
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
    VkPipeline getOrCreatePipeline() override;

   private:
    ShaderID _computeModuleID;
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
    VkPipeline getOrCreatePipeline() override;

   private:
    ShaderID _rayGenModuleID;
    ShaderID _rayCHitModuleID;
    ShaderID _rayAHitModuleID;
    ShaderID _rayMissModuleID;
    ShaderID _rayIntersectModuleID;
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
    VkPipeline getOrCreatePipeline() override;

   private:
    ShaderID _taskModuleID;
    ShaderID _meshModuleID;
    ShaderID _fragModuleID;
};
} // namespace Play

#endif // PLAYPROGRAM_H