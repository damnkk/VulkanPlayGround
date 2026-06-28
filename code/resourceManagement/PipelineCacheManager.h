#ifndef PLAY_PIPELINE_CACHE_MANAGER_H
#define PLAY_PIPELINE_CACHE_MANAGER_H
#include "Resource.h"
#include "DescriptorManager.h"
#include <nvvk/descriptors.hpp>
#include <nvvk/graphics_pipeline.hpp>
#include <functional>
#include "ShaderManager.hpp"
#include "RenderPass.h"
#include "core/DataWriter.h"
namespace Play
{
using BlockKey    = std::uint32_t;
using PipelineKey = std::size_t;

using CacheLRU                                       = std::list<BlockKey>;
const uint32_t              MAX_BLOCK_PIPELINE_COUNT = 30;
const std::filesystem::path PIPELINE_CACHE_PATH      = "pipelineCache";
const std::filesystem::path PIPELINE_CACHE_FILE_NAME = "pipelineCache.db";

const uint32_t BLOCK_STATE_INITED    = 1 << 0;
const uint32_t BLOCK_STATE_BUILDING  = 1 << 1;
const uint32_t BLOCK_STATE_CLOSING   = 1 << 2;
const uint32_t BLOCK_STATE_FINALIZED = 1 << 3;
const uint32_t BLOCK_STATE_EVICTED   = 1 << 4;

class PplCacheBlockManager;
class PplCacheBlock
{
public:
    uint32_t _state;
    PplCacheBlock(BlockKey key) : _blockKey(key) {}
    inline bool isInited() const
    {
        return _state == BLOCK_STATE_INITED;
    }
    inline bool isBuilding() const
    {
        return _state == BLOCK_STATE_BUILDING;
    }
    inline bool isFinalized() const
    {
        return _state == BLOCK_STATE_FINALIZED;
    }
    inline bool isEvicted() const
    {
        return _state & BLOCK_STATE_EVICTED;
    }
    inline bool isClosing() const
    {
        return _state == BLOCK_STATE_CLOSING;
    }
    bool loadFromDisk(bool fullLoading = false);

    void saveToDisk();

    void init();

    bool tryAdd(PipelineKey& key);

    void createPipeline(std::function<VkPipeline(PplCacheBlock*)>&& createFunc);

    void unLoad();

    inline std::string getBlockPath() const
    {
        return std::to_string(_blockKey) + ".bin";
    }
    uint32_t              _blockKey;
    uint32_t              _currentPipelineCount = 0;
    uint32_t              _pendingPipelineCount = 0;
    uint64_t              _currPsoCacheSize     = 0;
    std::vector<uint64_t> _pipelineKeys;
    VkPipelineCache       _vkHandle;
    std::mutex            _cacheLock;
    std::mutex            _stateLock;
    uint64_t              _lastAccessFrame = 0;
    PplCacheBlockManager* test             = nullptr;
};

class PplCacheBlockManager
{
public:
    PplCacheBlockManager();
    ~PplCacheBlockManager();
    void deinit() {}

    void Tick() {}

    void           tryToUnloadBlock() {}
    void           loadAllBlockFromDisk();
    PplCacheBlock* getOrCreateBlock(PipelineKey key);

private:
    friend class PplCacheBlock;
    void saveHeaderInfo();

    inline std::filesystem::path getRootInfoPath() const
    {
        return "rootInfo.bin";
    }
    struct HeaderInfo
    {
        uint32_t              deviceID;
        uint32_t              vendorID;
        uint8_t               pipelineCacheUUID[VK_UUID_SIZE];
        uint32_t              blockCnt = 0;
        std::vector<BlockKey> blockKeys;
        void                  initFromLoadRes(BufferStream& res);
    } _HeaderInfo;

    CacheLRU                                                     _lru;
    std::unordered_map<BlockKey, std::unique_ptr<PplCacheBlock>> _blockMap;
    std::unordered_map<PipelineKey, BlockKey>                    _pipelineToBlockMap;
    std::mutex                                                   _lock;
    uint32_t                                                     _nextBlockKey = 0;
};

using ShaderID = uint32_t;
class ComputePipelineState
{
public:
    PipelineKey getPipelineKey() const;
    void        addShader(ShaderModule* shaderModule)
    {
        _shaderModule = shaderModule;
    }

private:
    ShaderModule* _shaderModule;
    PipelineKey   _pipelineKey = ~0U;
};

class PSOState : public nvvk::GraphicsPipelineState
{
public:
    // Dynamic States configuration
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};
    VkPipelineCreateFlags2      flags2        = 0;
    // Calculate a hash key for caching
    PipelineKey getPipelineKey();
    bool        dirtyFlag   = true;
    PipelineKey pipelineKey = ~0U;
};


struct GraphicsShaderSet
{
    ShaderID vertexModuleID = ~0U;
    ShaderID fragModuleID   = ~0U;
    ShaderID taskModuleID   = ~0U;
    ShaderID meshModuleID   = ~0U;

    bool isMeshPipeline() const
    {
        return meshModuleID != ~0U;
    }

    PipelineKey getShaderKey() const;
};

struct RenderTargetState
{
    std::vector<VkFormat> colorFormats;
    VkFormat              depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
    VkFormat              stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits sampleCount              = VK_SAMPLE_COUNT_1_BIT;

    PipelineKey getPipelineKey() const;
};

class PipelineLayout
{
public:
    VkPipelineLayout vkHandle = VK_NULL_HANDLE;
    PipelineKey      hash     = 0;
    uint32_t         setCount = 0;
    std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorEnum::eCount)> setLayouts = {VK_NULL_HANDLE};
    VkPushConstantRange pushConstantRange = {};
    bool                hasPushConstant   = false;
};

class PipelineLayoutDesc
{
public:
    PipelineLayoutDesc& setDescriptorSetLayout(DescriptorEnum setSlot, VkDescriptorSetLayout layout);
    PipelineLayoutDesc& setDescriptorSet(DescriptorEnum setSlot, DescriptorSetBindings& descriptorSet);
    PipelineLayoutDesc& setPassDescriptorSet(DescriptorSetBindings& descriptorSet);
    PipelineLayoutDesc& setMaterialDescriptorSet(DescriptorSetBindings& descriptorSet);
    PipelineLayoutDesc& setPushConstantRange(const VkPushConstantRange& range);

    template <typename T>
    PipelineLayoutDesc& setPushConstant(VkShaderStageFlags stage = VK_SHADER_STAGE_ALL)
    {
        VkPushConstantRange range = {};
        range.stageFlags         = stage;
        range.offset             = 0;
        range.size               = static_cast<uint32_t>(sizeof(T));
        return setPushConstantRange(range);
    }

    PipelineKey getPipelineKey() const;
    uint32_t    getSetLayoutCount() const;

    const std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorEnum::eCount)>& getSetLayouts() const
    {
        return _setLayouts;
    }

    bool hasPushConstantRange() const
    {
        return _hasPushConstantRange;
    }

    const VkPushConstantRange& getPushConstantRange() const
    {
        return _pushConstantRange;
    }

private:
    std::array<VkDescriptorSetLayout, static_cast<size_t>(DescriptorEnum::eCount)> _setLayouts = {VK_NULL_HANDLE};
    VkPushConstantRange _pushConstantRange = {};
    bool                _hasPushConstantRange = false;
};

class PipelineLayoutCache
{
public:
    ~PipelineLayoutCache();
    PipelineLayout* getOrCreatePipelineLayout(const PipelineLayoutDesc& desc);

private:
    VkDescriptorSetLayout getEmptyDescriptorSetLayout();

    std::unordered_map<PipelineKey, std::unique_ptr<PipelineLayout>> _pipelineLayoutMap;
    VkDescriptorSetLayout _emptyDescriptorSetLayout = VK_NULL_HANDLE;
};

class GraphicsPipelineStateInitializer
{
public:
    GraphicsShaderSet  shaderSet;
    PSOState           psoState;
    RenderTargetState  renderTargetState;
    DescriptorSetBindings* passDescriptorSet     = nullptr;
    DescriptorSetBindings* materialDescriptorSet = nullptr;
    PipelineLayout*         pipelineLayout        = nullptr;
    VkPushConstantRange     pushConstantRange     = {};
    bool                    hasPushConstantRange  = false;

    GraphicsPipelineStateInitializer& setShader(ShaderID vertexModuleID, ShaderID fragModuleID);
    GraphicsPipelineStateInitializer& setMeshShader(ShaderID meshModuleID, ShaderID fragModuleID, ShaderID taskModuleID = ~0U);
    GraphicsPipelineStateInitializer& setPassDescriptorSet(DescriptorSetBindings& descriptorSet);
    GraphicsPipelineStateInitializer& setMaterialDescriptorSet(DescriptorSetBindings& descriptorSet);
    GraphicsPipelineStateInitializer& setPushConstantRange(const VkPushConstantRange& range);

    template <typename T>
    GraphicsPipelineStateInitializer& setPushConstant(VkShaderStageFlags stage = VK_SHADER_STAGE_ALL)
    {
        VkPushConstantRange range = {};
        range.stageFlags         = stage;
        range.offset             = 0;
        range.size               = static_cast<uint32_t>(sizeof(T));
        return setPushConstantRange(range);
    }

    PipelineKey getPipelineKey();
};

class ComputePipelineStateInitializer
{
public:
    ShaderID computeModuleID = ~0U;
    DescriptorSetBindings* passDescriptorSet     = nullptr;
    DescriptorSetBindings* materialDescriptorSet = nullptr;
    PipelineLayout*         pipelineLayout        = nullptr;
    VkPushConstantRange     pushConstantRange     = {};
    bool                    hasPushConstantRange  = false;

    ComputePipelineStateInitializer& setShader(ShaderID moduleID);
    ComputePipelineStateInitializer& setPassDescriptorSet(DescriptorSetBindings& descriptorSet);
    ComputePipelineStateInitializer& setMaterialDescriptorSet(DescriptorSetBindings& descriptorSet);
    ComputePipelineStateInitializer& setPushConstantRange(const VkPushConstantRange& range);

    template <typename T>
    ComputePipelineStateInitializer& setPushConstant(VkShaderStageFlags stage = VK_SHADER_STAGE_COMPUTE_BIT)
    {
        VkPushConstantRange range = {};
        range.stageFlags         = stage;
        range.offset             = 0;
        range.size               = static_cast<uint32_t>(sizeof(T));
        return setPushConstantRange(range);
    }

    PipelineKey getPipelineKey();
};
class RTPipelineState
{
public:
    ShaderID _rayGenModuleID       = ~0U;
    ShaderID _rayCHitModuleID      = ~0U;
    ShaderID _rayAHitModuleID      = ~0U;
    ShaderID _rayMissModuleID      = ~0U;
    ShaderID _rayIntersectModuleID = ~0U;
};
class PipelineCacheManager
{
public:
    PipelineCacheManager();
    virtual ~PipelineCacheManager();
    VkPipeline getOrCreateGraphicsPipeline(GraphicsPipelineStateInitializer& initializer);
    VkPipeline getOrCreateComputePipeline(ComputePipelineStateInitializer& initializer);
    PipelineLayout* getOrCreatePipelineLayout(const PipelineLayoutDesc& desc);
    VkPipeline getOrCreateRTPipeline(RTPipelineState& rtState);
    VkPipeline getOrCreateMeshPipeline(PSOState& psoState, RenderPass* renderPass, ShaderID mShaderID, ShaderID fShaderID, ShaderID tShaderID = ~0U);

private:
    std::unique_ptr<PplCacheBlockManager>    _cacheBlockManager = nullptr;
    nvvk::GraphicsPipelineCreator            _gfxPipelineCreator;
    PipelineLayoutCache                      _pipelineLayoutCache;
    std::unordered_map<uint64_t, VkPipeline> _pipelineMap;
};

} // namespace Play

#endif // PLAY_PIPELINE_CACHE_MANAGER_H