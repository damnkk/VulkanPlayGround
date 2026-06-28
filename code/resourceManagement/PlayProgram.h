#ifndef PLAYPROGRAM_H
#define PLAYPROGRAM_H

#include "core/runtime/RenderSession.h"
#include <nvvk/descriptors.hpp>
#include <nvvk/graphics_pipeline.hpp>
#include <nvvk/compute_pipeline.hpp>
#include "PipelineCacheManager.h"
#include <nvvk/pipeline.hpp>
#include <ShaderManager.hpp>
#include "DescriptorManager.h"
#include "RenderPass.h"
#include "PConstantType.h.slang"
#include "core/runtime/VulkanRuntime.h"
#include "core/RefCounted.h"
#include <rttr/rttr_enable.h>
namespace Play
{
namespace RDG
{
class PassNode;
class RenderPassNode;
struct PendingState;
} // namespace RDG
using ShaderID = uint32_t;

class DescriptorSetManager : public DescriptorSetBindings
{
public:
    DescriptorSetManager();
    ~DescriptorSetManager();

    void finalizePipelineLayout();

    // push constants
    template <typename T>
    DescriptorSetManager& initPushConstant(VkShaderStageFlags stage = VK_SHADER_STAGE_ALL)
    {
        uint32_t size    = static_cast<uint32_t>(sizeof(T));
        uint32_t maxSize = vkDriver->_physicalDeviceProperties2.properties.limits.maxPushConstantsSize;
        if (size > maxSize)
        {
            LOGE(("Push constant size limit exceeded! Max: " + std::to_string(maxSize) + ", Requested: " + std::to_string(size)).c_str());
        }

        _pipelineLayoutDirty |= 1 << 1;
        _pushConstantRange            = {};
        _pushConstantRange.stageFlags = stage;
        _pushConstantRange.offset     = 0;
        _pushConstantRange.size       = size;
        _hasPushConstantRange         = true;
        return *this;
    }

    template <typename T>
    T createPushConstant() const
    {
        if (!_hasPushConstantRange)
        {
            LOGE("Push constant range is not registered");
        }
        else if (_pushConstantRange.size != static_cast<uint32_t>(sizeof(T)))
        {
            LOGE("Push constant type size mismatch");
        }
        return {};
    }

    bool hasPushConstantRange() const
    {
        return _hasPushConstantRange;
    }

    const VkPushConstantRange& getPushConstantRange() const
    {
        return _pushConstantRange;
    }

    // getter
    VkPipelineLayout getPipelineLayout() const;

    const std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorEnum::eCount)>& getDescriptorSetLayouts()
    {
        return _descSetLayouts;
    }

    const VkDescriptorSetLayout& getDescriptorSetLayout(DescriptorEnum setEnum)
    {
        assert(setEnum != DescriptorEnum::eCount);
        return _descSetLayouts[uint32_t(setEnum)];
    }

    void setDescriptorSetLayout(DescriptorEnum setEnum, VkDescriptorSetLayout layout)
    {
        assert(setEnum != DescriptorEnum::eCount);
        if (_descSetLayouts[uint32_t(setEnum)] == layout) return;
        _pipelineLayoutDirty |= 1 << 1;
        _descSetLayouts[uint32_t(setEnum)] = layout;
    }

private:
    friend class PlayProgram; // 允许 PlayProgram 访问私有成员进行延迟销毁
    void finalizePipelineLayoutImpl();
    DescriptorSetManager(const DescriptorSetManager&);
    DescriptorSetManager& operator=(const DescriptorSetManager&);

    // for binding request merge
    std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorEnum::eCount)> _descSetLayouts      = {VK_NULL_HANDLE};
    VkPipelineLayout                                                               _pipelineLayout      = VK_NULL_HANDLE;
    uint8_t                                                                        _pipelineLayoutDirty = 0;
    VkPushConstantRange                                                            _pushConstantRange   = {};
    bool                                                                           _hasPushConstantRange = false;
};

enum class ProgramType
{
    eRenderProgram,
    eComputeProgram,
    eRTProgram,
    eMeshRenderProgram,
    eUndefined
};

class PlayProgram : public RefCounted
{
public:
    PlayProgram()
    {
        if (vkDriver) vkDriver->registerObject(this);
    }
    virtual ~PlayProgram()
    {
        // 由 onDestroy() 处理资源清理
    }

    PlayProgram(const PlayProgram&)            = delete;
    PlayProgram& operator=(const PlayProgram&) = delete;

    virtual ProgramType   getProgramType() const = 0;
    DescriptorSetManager& getDescriptorSetManager()
    {
        return _descriptorSetManager;
    }
    template <typename T>
    PlayProgram& initPushConstant(VkShaderStageFlags stage = VK_SHADER_STAGE_ALL)
    {
        _descriptorSetManager.initPushConstant<T>(stage);
        return *this;
    }
    template <typename T>
    T createPushConstant() const
    {
        return _descriptorSetManager.createPushConstant<T>();
    }
    bool hasPushConstantRange() const
    {
        return _descriptorSetManager.hasPushConstantRange();
    }
    const VkPushConstantRange& getPushConstantRange() const
    {
        return _descriptorSetManager.getPushConstantRange();
    }
    VkPipelineLayout getPipelineLayout() const
    {
        return _descriptorSetManager.getPipelineLayout();
    }
    VkPipelineBindPoint getPipelineBindPoint() const
    {
        if (_programType == ProgramType::eRenderProgram || _programType == ProgramType::eMeshRenderProgram)
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
    RTTR_ENABLE(RefCounted)

protected:
    friend struct RDG::PendingState;
    virtual void setPassNode(RDG::PassNode* passNode);
    virtual void bindPipeline(VkCommandBuffer cmdBuf) = 0;
    virtual void finish()
    {
        _descriptorSetManager.finalizePipelineLayout();
    };
    void onDestroy() override;

    RDG::PassNode*       _passNode = nullptr;
    DescriptorSetManager _descriptorSetManager;
    ProgramType          _programType = ProgramType::eUndefined;
};

class RenderProgram : public PlayProgram
{
public:
    RenderProgram()
    {
        _programType = ProgramType::eRenderProgram;
    }
    RenderProgram(ShaderID vertexModuleID, ShaderID fragModuleID) : _vertexModuleID(vertexModuleID), _fragModuleID(fragModuleID)
    {
        _programType = ProgramType::eRenderProgram;
    }
    ~RenderProgram() override {}
    RenderProgram& setVertexModuleID(ShaderID vertexModuleID);
    ShaderID       getVertexModuleID() const
    {
        return _vertexModuleID;
    }
    ShaderID getFragModuleID() const
    {
        return _fragModuleID;
    }
    RenderProgram& setFragModuleID(ShaderID fragModuleID);

    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

    RDG::RenderPassNode* getPassNode();

    PSOState& psoState()
    {
        return _psoState;
    }

    RTTR_ENABLE(PlayProgram)

protected:
    void bindPipeline(VkCommandBuffer cmdBuf) override;

private:
    VkPipeline getOrCreatePipeline();
    ShaderID   _vertexModuleID = ~0U;
    ShaderID   _fragModuleID   = ~0U;

    PSOState _psoState;
};

class ComputeProgram : public PlayProgram
{
public:
    ComputeProgram()
    {
        _programType = ProgramType::eComputeProgram;
    }
    ComputeProgram(ShaderID computeModuleID) : _computeModuleID(computeModuleID)
    {
        _programType = ProgramType::eComputeProgram;
    }
    ComputeProgram& setComputeModuleID(ShaderID computeModuleID);
    ShaderID        getComputeModuleID() const
    {
        return _computeModuleID;
    }

    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

    RTTR_ENABLE(PlayProgram)

protected:
    void bindPipeline(VkCommandBuffer cmdBuf) override;

private:
    VkPipeline getOrCreatePipeline();
    ShaderID   _computeModuleID = ~0U;
};

class RTProgram : public PlayProgram
{
public:
    RTProgram()
    {
        _programType = ProgramType::eRTProgram;
    }
    RTProgram(VkDevice device, ShaderID rayGenID, ShaderID rayCHitID, ShaderID rayMissID)
        : _rayGenModuleID(rayGenID), _rayCHitModuleID(rayCHitID), _rayMissModuleID(rayMissID)
    {
        _programType = ProgramType::eRTProgram;
    }
    RTProgram& setRayGenModuleID(ShaderID rayGenModuleID);
    RTProgram& setRayCHitModuleID(ShaderID rayCHitModuleID);
    RTProgram& setRayAHitModuleID(ShaderID rayAHitModuleID);
    RTProgram& setRayMissModuleID(ShaderID rayMissModuleID);
    RTProgram& setRayIntersectModuleID(ShaderID rayIntersectModuleID);

    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

    RTTR_ENABLE(PlayProgram)

protected:
    void bindPipeline(VkCommandBuffer cmdBuf) override;

private:
    VkPipeline getOrCreatePipeline();
    ShaderID   _rayGenModuleID       = ~0U;
    ShaderID   _rayCHitModuleID      = ~0U;
    ShaderID   _rayAHitModuleID      = ~0U;
    ShaderID   _rayMissModuleID      = ~0U;
    ShaderID   _rayIntersectModuleID = ~0U;
};

class MeshRenderProgram : public PlayProgram
{
public:
    MeshRenderProgram()
    {
        _programType = ProgramType::eMeshRenderProgram;
    }
    MeshRenderProgram(VkDevice device, ShaderID meshModuleID, ShaderID fragModuleID) : _meshModuleID(meshModuleID), _fragModuleID(fragModuleID)
    {
        _programType = ProgramType::eMeshRenderProgram;
    }
    MeshRenderProgram& setTaskModuleID(ShaderID taskModuleID);
    MeshRenderProgram& setMeshModuleID(ShaderID meshModuleID);
    MeshRenderProgram& setFragModuleID(ShaderID fragModuleID);
    ShaderID           getTaskModuleID() const
    {
        return _taskModuleID;
    }
    ShaderID getMeshModuleID() const
    {
        return _meshModuleID;
    }
    ShaderID getFragModuleID() const
    {
        return _fragModuleID;
    }

    RDG::RenderPassNode* getPassNode();

    PSOState& psoState()
    {
        return _psoState;
    }

    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

    RTTR_ENABLE(PlayProgram)

protected:
    void bindPipeline(VkCommandBuffer cmdBuf) override;

private:
    VkPipeline getOrCreatePipeline();
    ShaderID   _taskModuleID = ~0U;
    ShaderID   _meshModuleID = ~0U;
    ShaderID   _fragModuleID = ~0U;
    PSOState   _psoState;
};
} // namespace Play

#endif // PLAYPROGRAM_H
