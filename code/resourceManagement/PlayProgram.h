#ifndef PLAYPROGRAM_H
#define PLAYPROGRAM_H

#include "PlayApp.h"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/shadermodulemanager_vk.hpp"

namespace Play
{
typedef nvvk::ShaderModuleID ShaderID;

struct BindInfo{
    uint32_t setIdx;
    uint32_t bindingIdx;
    uint32_t descriptorCount;
    VkDescriptorType descriptorType;
    VkPipelineStageFlags2 pipelineStageFlags;
};

struct ConstantRange{
    size_t size;
    VkPipelineStageFlags2 stage;
};
/*
Example:

    VkDevice device;
    DescriptorSetManager descriptorManager(device);
    descriptorManager.addBinding(1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    descriptorManager.addBinding(1, 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    descriptorManager.initLayout(1);
    descriptorManager.addBinding(2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    descriptorManager.addBinding(2,0,3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    descriptorManager.initLayout(2);
    descriptorManager.addConstantRange(64, 0, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    descriptorManager.finish();
    VkPipelineLayout layout1 = descriptorManager.getPipelineLayout();
    descriptorManager.at(1).initPool(1);
    descriptorManager.at(2).initPool(1);
    VkDescriptorImageInfo tempImageInfo;
    VkWriteDescriptorSet write =  descriptorManager.at(1).makeWrite(0,1,&tempImageInfo);
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
*/

class DescriptorSetManager : public nvvk::TDescriptorSetContainer<MAX_DESCRIPTOR_SETS::value,1>
{
public:
    DescriptorSetManager(VkDevice device);
    ~DescriptorSetManager();
    DescriptorSetManager& addBinding(uint32_t setIdx, uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType, VkPipelineStageFlags2 pipelineStageFlags);
    DescriptorSetManager& addBinding(const BindInfo& bindingInfo);
    DescriptorSetManager& initLayout(uint32_t setIdx);
    bool finish();
    DescriptorSetManager& addConstantRange(uint32_t size,uint32_t offset, VkPipelineStageFlags2 stage);

    VkPipelineLayout getPipelineLayout() const;
private:
    DescriptorSetManager(const DescriptorSetManager&);
    DescriptorSetManager& operator=(const DescriptorSetManager&);

    VkDevice getDevice() const { return m_sets[0].getDevice(); }
    bool _recordState = false; //记录状态,保证binding有增改之后会调用finish,刷新管线布局
    std::vector<BindInfo> _bindingInfos;
    std::vector<VkPushConstantRange> _constantRanges;
    // Additional functionality for managing descriptor sets can be added here
};

class PlayProgram
{
public:
    PlayProgram();
    ~PlayProgram();
    PlayProgram(const PlayProgram&);
    PlayProgram& operator=(const PlayProgram&);
    virtual VkPipeline getOrCreatePipeline() = 0;

protected:
    DescriptorSetManager _descriptorManager;
};

class RenderProgram: public PlayProgram{
public:
    RenderProgram() = default;
    RenderProgram(ShaderID vertexModuleID, ShaderID fragModuleID)
        : _vertexModuleID(vertexModuleID), _fragModuleID(fragModuleID) {}
    void setVertexModuleID(ShaderID vertexModuleID) {_vertexModuleID = vertexModuleID;}
    void setFragModuleID(ShaderID fragModuleID) {_fragModuleID = fragModuleID;}
    VkPipeline getOrCreatePipeline() override;
private:
    ShaderID _vertexModuleID;
    ShaderID _fragModuleID;
};

class ComputeProgram: public PlayProgram{
public:
    ComputeProgram() = default;
    ComputeProgram(ShaderID computeModuleID)
        : _computeModuleID(computeModuleID) {}
    void setComputeModuleID(ShaderID computeModuleID) {_computeModuleID = computeModuleID;}
    VkPipeline getOrCreatePipeline() override;
private:
    nvvk::ShaderModuleID _computeModuleID;
};

class RTProgram:public PlayProgram{
public:
    RTProgram() = default;
    RTProgram(ShaderID rayGenID, ShaderID rayCHitID, ShaderID rayMissID)
        : _rayGenModuleID(rayGenID), _rayCHitModuleID(rayCHitID), _rayMissModuleID(rayMissID) {}
    void setRayGenModuleID(ShaderID rayGenModuleID) {_rayGenModuleID = rayGenModuleID;}
    void setRayCHitModuleID(ShaderID rayCHitModuleID) {_rayCHitModuleID = rayCHitModuleID;}
    void setRayAHitModuleID(ShaderID rayAHitModuleID) {_rayAHitModuleID = rayAHitModuleID;}
    void setRayMissModuleID(ShaderID rayMissModuleID) {_rayMissModuleID = rayMissModuleID;}
    void setRayIntersectModuleID(ShaderID rayIntersectModuleID) {_rayIntersectModuleID = rayIntersectModuleID;}
    VkPipeline getOrCreatePipeline() override;
private:
    nvvk::ShaderModuleID _rayGenModuleID;
    nvvk::ShaderModuleID _rayCHitModuleID;
    nvvk::ShaderModuleID _rayAHitModuleID;
    nvvk::ShaderModuleID _rayMissModuleID;
    nvvk::ShaderModuleID _rayIntersectModuleID;
};

class MeshRenderProgram:public PlayProgram{
public:
    MeshRenderProgram() = default;
    MeshRenderProgram(ShaderID meshModuleID, ShaderID fragModuleID)
        : _meshModuleID(meshModuleID), _fragModuleID(fragModuleID) {}
    void setTaskModuleID(ShaderID taskModuleID) {_taskModuleID = taskModuleID;}
    void setMeshModuleID(ShaderID meshModuleID) {_meshModuleID = meshModuleID;}
    void setFragModuleID(ShaderID fragModuleID) {_fragModuleID = fragModuleID;}
    VkPipeline getOrCreatePipeline() override;
private:
    nvvk::ShaderModuleID _taskModuleID;
    nvvk::ShaderModuleID _meshModuleID;
    nvvk::ShaderModuleID _fragModuleID; 
};
} // namespace Play

#endif // PLAYPROGRAM_H