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
namespace Play
{
using ShaderID = uint32_t;

struct BindInfo
{
    uint32_t           setIdx;
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
    void addRange(VkShaderStageFlags stage)
    {
        std::type_index typeIdx(typeid(T));
        auto            it = _typeMap.find(typeIdx);
        if (it != _typeMap.end())
        {
            // 如果存在，合并 stageFlags
            _ranges[it->second].stageFlags |= stage;
            return;
        }

        uint32_t size = static_cast<uint32_t>(sizeof(T));

        // 4字节对齐 (Vulkan Spec 要求 offset 必须是 4 的倍数)
        uint32_t alignedOffset = static_cast<uint32_t>((_currOffset + 3) & ~3);

        // 3. 检查剩余空间
        if (alignedOffset + size > _maxSize)
        {
            LOGE(("Push constant size limit exceeded! Max: " + std::to_string(_maxSize) + ", Requested end: " + std::to_string(alignedOffset + size))
                     .c_str());
        }

        // 4. 创建新 Range
        VkPushConstantRange range{};
        range.stageFlags = stage;
        range.offset     = alignedOffset;
        range.size       = size;

        _ranges.emplace_back(stage, alignedOffset, size);

        // 记录映射关系
        _typeMap[typeIdx] = static_cast<uint32_t>(_ranges.size() - 1);

        // 更新当前偏移量
        _currOffset = alignedOffset + size;
    }
    template <typename T>
    T* getRange()
    {
        std::type_index typeIdx(typeid(T));
        auto            it = _typeMap.find(typeIdx);
        if (it != _typeMap.end())
        {
            LOGE("PushConstantManager::getRange: Type not found");
        }
        return static_cast<T*>(&_constantData[_ranges[it->second].offset]);
    }

    const std::vector<VkPushConstantRange>& getRanges() const;

    uint32_t getMaxSize() const;

    void clear();

private:
    uint32_t                                      _maxSize;
    size_t                                        _currOffset = 0;
    std::vector<VkPushConstantRange>              _ranges;
    std::unordered_map<std::type_index, uint32_t> _typeMap;
    std::vector<uint8_t>                          _constantData;
};

class DescriptorSetManager
{
public:
    DescriptorSetManager(VkDevice device);
    ~DescriptorSetManager();
    void                  deinit();
    DescriptorSetManager& addBinding(uint32_t setIdx, uint32_t bindingIdx, uint32_t descriptorCount, VkDescriptorType descriptorType,
                                     VkShaderStageFlags shaderStageFlags);
    DescriptorSetManager& addBinding(const BindInfo& bindingInfo);
    bool                  finalizeLayout();
    template <typename T>
    DescriptorSetManager& addConstantRange(VkShaderStageFlags stage)
    {
        _constantRanges.addRange<T>(stage);
        return *this;
    }

    template <typename T>
    T* getConstantRange()
    {
        return _constantRanges.getRange<T>();
    }

    VkPipelineLayout                                                                   getPipelineLayout() const;
    std::array<nvvk::DescriptorBindings, static_cast<size_t>(DescriptorEnum::eCount)>& getSetBindingInfo()
    {
        return _descBindSet;
    }

    std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorEnum::eCount)>& getDescriptorSetLayouts()
    {
        return _descSetLayouts;
    }

    VkDescriptorSetLayout getDescriptorSetLayout(DescriptorEnum setEnum)
    {
        assert(setEnum != DescriptorEnum::eCount);
        return _descSetLayouts[uint32_t(setEnum)];
    }

    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::Buffer& buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::AccelerationStructure& accel);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::Image& image);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkDescriptorBufferInfo& bufferInfo);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, VkImageView imageView, VkImageLayout imageLayout, VkSampler sampler = nullptr);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkDescriptorImageInfo& imageInfo);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, VkAccelerationStructureKHR accel);

    // writeSet.descriptorCount many elements
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::Buffer* buffers,
                     uint32_t count); // offset 0 and VK_WHOLE_SIZE
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::AccelerationStructure* accels, uint32_t count);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const nvvk::Image* images, uint32_t count);

    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkDescriptorBufferInfo* bufferInfos, uint32_t count);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkDescriptorImageInfo* imageInfos, uint32_t count);
    void setDescInfo(uint32_t setIdx, uint32_t bindingIdx, const VkAccelerationStructureKHR* accels, uint32_t count);

    uint64_t getBindingsHash(uint32_t setIdx);
    uint64_t getDescsetLayoutHash(uint32_t setIdx);
    union DescriptorInfo
    {
        VkDescriptorBufferInfo     buffer;
        VkDescriptorImageInfo      image;
        VkAccelerationStructureKHR accel;
    };

    std::vector<DescriptorInfo> getDescriptorInfo(uint32_t setIdx);

    int descriptorOffset(uint32_t setIdx, uint32_t bindingIdx) const;

private:
    DescriptorSetManager& initLayout();
    DescriptorSetManager(const DescriptorSetManager&);
    DescriptorSetManager& operator=(const DescriptorSetManager&);

    VkDevice getDevice() const
    {
        return _vkDevice;
    }
    bool                  _isRecorded = false; // 记录状态,保证binding有增改之后会调用finish,刷新管线布局
    std::vector<BindInfo> _bindingInfos;
    PushConstantManager   _constantRanges;
    std::array<nvvk::DescriptorBindings, static_cast<size_t>(DescriptorEnum::eCount)> _descBindSet;
    std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorEnum::eCount)>    _descSetLayouts = {};
    VkPipelineLayout                                                                  _pipelineLayout = VK_NULL_HANDLE;
    VkDevice                                                                          _vkDevice;
    uint8_t _dirtyFlags = 0; // bit0: binding changed, bit1: constant range changed

    std::array<std::vector<DescriptorInfo>, static_cast<size_t>(DescriptorEnum::eCount) - static_cast<size_t>(DescriptorEnum::ePerPassDescriptorSet)>
                                                                                                                                   _descInfos;
    std::array<uint64_t, static_cast<size_t>(DescriptorEnum::eCount) - static_cast<size_t>(DescriptorEnum::ePerPassDescriptorSet)> _setBindingHashs =
        {};
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
    PlayProgram(VkDevice device) : _descriptorSetManager(device) {}
    virtual ~PlayProgram()
    {
        _descriptorSetManager.deinit();
    }

    PlayProgram(const PlayProgram&);
    PlayProgram&          operator=(const PlayProgram&);
    virtual ProgramType   getProgramType() const = 0;
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
    virtual void finish()
    {
        _descriptorSetManager.addConstantRange<PerFrameConstant>(VK_SHADER_STAGE_ALL);
        _descriptorSetManager.finalizeLayout();
    };

protected:
    DescriptorSetManager _descriptorSetManager;
    ProgramType          _programType = ProgramType::eUndefined;
};

class RenderProgram : public PlayProgram
{
public:
    RenderProgram(VkDevice device) : PlayProgram(device) {}
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
    void setRenderPass(RenderPass* renderPass)
    {
        _renderPass = renderPass;
    }
    RenderPass* getRenderPass()
    {
        return _renderPass;
    }
    PSOState& psoState()
    {
        return _psoState;
    }

private:
    VkPipeline  getOrCreatePipeline(RenderPass* renderPass);
    ShaderID    _vertexModuleID = ~0U;
    ShaderID    _fragModuleID   = ~0U;
    ProgramType _programType    = ProgramType::eRenderProgram;
    RenderPass* _renderPass     = nullptr;
    PSOState    _psoState;
};

class ComputeProgram : public PlayProgram
{
public:
    ComputeProgram(VkDevice device) : PlayProgram(device) {}
    ComputeProgram(VkDevice device, ShaderID computeModuleID) : PlayProgram(device), _computeModuleID(computeModuleID) {}
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
    RTProgram(VkDevice device) : PlayProgram(device) {}
    RTProgram(VkDevice device, ShaderID rayGenID, ShaderID rayCHitID, ShaderID rayMissID)
        : PlayProgram(device), _rayGenModuleID(rayGenID), _rayCHitModuleID(rayCHitID), _rayMissModuleID(rayMissID)
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
    MeshRenderProgram(VkDevice device) : PlayProgram(device) {}
    MeshRenderProgram(VkDevice device, ShaderID meshModuleID, ShaderID fragModuleID)
        : PlayProgram(device), _meshModuleID(meshModuleID), _fragModuleID(fragModuleID)
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