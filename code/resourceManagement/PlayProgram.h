#ifndef PLAYPROGRAM_H
#define PLAYPROGRAM_H

#include "PlayApp.h"
#include <nvvk/descriptors.hpp>
#include <nvvk/graphics_pipeline.hpp>
#include <nvvk/compute_pipeline.hpp>
#include "PipelineCacheManager.h"
#include <nvvk/pipeline.hpp>
#include <ShaderManager.hpp>
#include "DescriptorManager.h"
#include "RenderPass.h"
#include "PConstantType.h.slang"
#include "VulkanDriver.h"
namespace Play
{
namespace RDG
{
class PassNode;
class RenderPassNode;
} // namespace RDG
using ShaderID = uint32_t;

struct BindInfo
{
    uint32_t           bindingIdx;
    uint32_t           descriptorCount;
    VkDescriptorType   descriptorType;
    VkShaderStageFlags shaderStageFlags;
};
#include "PConstantType.h.slang"

class PushConstantManager
{
public:
    PushConstantManager();
    template <typename T>
    void registConstantType(VkShaderStageFlags stage = VK_SHADER_STAGE_ALL)
    {
        uint32_t size = static_cast<uint32_t>(sizeof(T));

        if (size > _maxSize)
        {
            LOGE(("Push constant size limit exceeded! Max: " + std::to_string(_maxSize) + ", Requested: " + std::to_string(size)).c_str());
        }

        _range            = {};
        _range.stageFlags = stage;
        _range.offset     = 0;
        _range.size       = size;
        _constantData.resize(size, 0);
    }

    template <typename T>
    T* getData()
    {
        return reinterpret_cast<T*>(_constantData.data());
    }

    void                 pushConstantRanges(VkCommandBuffer cmdBuf, VkPipelineLayout layout);
    VkPushConstantRange& getRange()
    {
        return _range;
    }

    uint32_t getMaxSize() const;
    void     clear();
    bool     haveRange() const
    {
        return !_constantData.empty();
    }

private:
    uint32_t             _maxSize;
    VkPushConstantRange  _range = {};
    std::vector<uint8_t> _constantData;
};

union DescriptorInfo
{
    VkDescriptorBufferInfo     buffer;
    VkDescriptorImageInfo      image;
    VkAccelerationStructureKHR accel;
};

class DescriptorSetBindings : public nvvk::DescriptorBindings
{
public:
    DescriptorSetBindings();
    ~DescriptorSetBindings();
    DescriptorSetBindings& addBinding(uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType,
                                      VkShaderStageFlags shaderStageFlags);
    DescriptorSetBindings& addBinding(const BindInfo& bindingInfo);
    void                   setDescInfo(uint32_t bindingIdx, const nvvk::Buffer& buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);

    void setDescInfo(uint32_t bindingIdx, const nvvk::Image& image);
    void setDescInfo(uint32_t bindingIdx, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);
    void setDescInfo(uint32_t bindingIdx, const VkDescriptorBufferInfo& bufferInfo);
    void setDescInfo(uint32_t bindingIdx, VkImageView imageView, VkImageLayout imageLayout, VkSampler sampler = nullptr);
    void setDescInfo(uint32_t bindingIdx, const VkDescriptorImageInfo& imageInfo);
    void setDescInfo(uint32_t bindingIdx, VkAccelerationStructureKHR accel);

    // writeSet.descriptorCount many elements
    void                         setDescInfo(uint32_t bindingIdx, const nvvk::Buffer* buffers, uint32_t count); // offset 0 and VK_WHOLE_SIZE
    void                         setDescInfo(uint32_t bindingIdx, const nvvk::Image* images, uint32_t count);
    void                         setDescInfo(uint32_t bindingIdx, const VkDescriptorBufferInfo* bufferInfos, uint32_t count);
    void                         setDescInfo(uint32_t bindingIdx, const VkDescriptorImageInfo* imageInfos, uint32_t count);
    void                         setDescInfo(uint32_t bindingIdx, const VkAccelerationStructureKHR* accels, uint32_t count);
    int                          descriptorOffset(uint32_t bindingIdx);
    uint64_t                     getBindingsHash(); // flush dirty flag
    uint64_t                     getDescsetLayoutHash();
    VkDescriptorSetLayout        finalizeLayout(); // flush recorded flag
    const VkDescriptorSetLayout& getSetLayout()
    {
        return _layout;
    }

    const std::vector<DescriptorInfo>& getDescriptorInfos()
    {
        return _descInfos;
    }

protected:
    std::vector<BindInfo>       _bindingInfos;
    uint64_t                    _setBindingHash;
    VkDescriptorSetLayout       _layout = VK_NULL_HANDLE;
    std::vector<DescriptorInfo> _descInfos;

private:
    bool    _setLayoutDirty = true; // layout changing state
    uint8_t _descInfoDirty  = 0;    // descinfo changing state | bit0: binding changed, bit1: constant range changed
};

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
        _pipelineLayoutDirty |= 1 << 1;
        _constantRanges.registConstantType<T>(stage);
        return *this;
    }

    template <typename T>
    T* getPushConstantData()
    {
        return _constantRanges.getData<T>();
    }

    void pushConstantRanges(VkCommandBuffer cmdBuf)
    {
        if (_constantRanges.haveRange()) _constantRanges.pushConstantRanges(cmdBuf, _pipelineLayout);
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
    void finalizePipelineLayoutImpl();
    DescriptorSetManager(const DescriptorSetManager&);
    DescriptorSetManager& operator=(const DescriptorSetManager&);
    PushConstantManager   _constantRanges;

    // for binding request merge
    std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorEnum::eCount)> _descSetLayouts      = {VK_NULL_HANDLE};
    VkPipelineLayout                                                               _pipelineLayout      = VK_NULL_HANDLE;
    uint8_t                                                                        _pipelineLayoutDirty = 0;
}; // Helper class to manage push constants.

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
    PlayProgram(uint32_t poolID) : poolId(poolID) {}
    virtual ~PlayProgram()
    {
        // _descriptorSetManager.deinit();
    }

    PlayProgram(const PlayProgram&);
    PlayProgram&          operator=(const PlayProgram&);
    virtual ProgramType   getProgramType() const = 0;
    virtual void          setPassNode(RDG::PassNode* passNode);
    DescriptorSetManager& getDescriptorSetManager()
    {
        return _descriptorSetManager;
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
    virtual void bind(VkCommandBuffer cmdBuf) = 0;
    // if you don't want user explicitly call finish(),you should xx it private
    virtual void finish()
    {
        _descriptorSetManager.finalizePipelineLayout();
    };

    uint32_t poolId = -1;

protected:
    RDG::PassNode*       _passNode = nullptr;
    DescriptorSetManager _descriptorSetManager;
    ProgramType          _programType = ProgramType::eUndefined;
};

class RenderProgram : public PlayProgram
{
public:
    RenderProgram(uint32_t id) : PlayProgram(id) {}
    RenderProgram(uint32_t id, ShaderID vertexModuleID, ShaderID fragModuleID)
        : PlayProgram(id), _vertexModuleID(vertexModuleID), _fragModuleID(fragModuleID)
    {
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

    void                bind(VkCommandBuffer cmdBuf) override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

    RDG::RenderPassNode* getPassNode();

    PSOState& psoState()
    {
        return _psoState;
    }

private:
    VkPipeline  getOrCreatePipeline();
    ShaderID    _vertexModuleID = ~0U;
    ShaderID    _fragModuleID   = ~0U;
    ProgramType _programType    = ProgramType::eRenderProgram;

    PSOState _psoState;
};

class ComputeProgram : public PlayProgram
{
public:
    ComputeProgram(uint32_t id) : PlayProgram(id) {}
    ComputeProgram(uint32_t id, VkDevice device, ShaderID computeModuleID) : PlayProgram(id), _computeModuleID(computeModuleID) {}
    ComputeProgram& setComputeModuleID(ShaderID computeModuleID);

    void                bind(VkCommandBuffer cmdBuf) override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    VkPipeline  getOrCreatePipeline();
    ShaderID    _computeModuleID = ~0U;
    ProgramType _programType     = ProgramType::eComputeProgram;
};

class RTProgram : public PlayProgram
{
public:
    RTProgram(uint32_t id) : PlayProgram(id) {}
    RTProgram(uint32_t id, VkDevice device, ShaderID rayGenID, ShaderID rayCHitID, ShaderID rayMissID)
        : PlayProgram(id), _rayGenModuleID(rayGenID), _rayCHitModuleID(rayCHitID), _rayMissModuleID(rayMissID)
    {
    }
    RTProgram& setRayGenModuleID(ShaderID rayGenModuleID);
    RTProgram& setRayCHitModuleID(ShaderID rayCHitModuleID);
    RTProgram& setRayAHitModuleID(ShaderID rayAHitModuleID);
    RTProgram& setRayMissModuleID(ShaderID rayMissModuleID);
    RTProgram& setRayIntersectModuleID(ShaderID rayIntersectModuleID);

    void                bind(VkCommandBuffer cmdBuf) override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    VkPipeline  getOrCreatePipeline();
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
    MeshRenderProgram(uint32_t id) : PlayProgram(id) {}
    MeshRenderProgram(uint32_t id, VkDevice device, ShaderID meshModuleID, ShaderID fragModuleID)
        : PlayProgram(id), _meshModuleID(meshModuleID), _fragModuleID(fragModuleID)
    {
    }
    MeshRenderProgram& setTaskModuleID(ShaderID taskModuleID);
    MeshRenderProgram& setMeshModuleID(ShaderID meshModuleID);
    MeshRenderProgram& setFragModuleID(ShaderID fragModuleID);

    void                bind(VkCommandBuffer cmdBuf) override;
    virtual ProgramType getProgramType() const override
    {
        return _programType;
    }

private:
    VkPipeline  getOrCreatePipeline();
    ShaderID    _taskModuleID = ~0U;
    ShaderID    _meshModuleID = ~0U;
    ShaderID    _fragModuleID = ~0U;
    ProgramType _programType  = ProgramType::eMeshRenderProgram;
};
} // namespace Play

#endif // PLAYPROGRAM_H