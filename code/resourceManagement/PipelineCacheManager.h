#ifndef PLAY_PIPELINE_CACHE_MANAGER_H
#define PLAY_PIPELINE_CACHE_MANAGER_H
#include "Resource.h"
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

    bool tryAdd();

    void createPipeline(std::function<void(VkPipelineCache)> createFunc);

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

    void deinit() {}

    void Tick() {}

    void tryToUnloadBlock() {}
    void loadAllBlockFromDisk();

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

    PplCacheBlock* getOrCreateBlock(PipelineKey key);

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
    VkPipeline getOrCreateGraphicsPipeline(const PSOState& psoState, RenderPass* renderPass, ShaderID vShaderID, ShaderID fShaderID);
    VkPipeline getOrCreateComputePipeline(const ComputePipelineState& computeState);
    VkPipeline getOrCreateRTPipeline(const RTPipelineState& rtState);
    VkPipeline getOrCreateMeshPipeline(const PSOState& psoState, RenderPass* renderPass, ShaderID mShaderID, ShaderID fShaderID,
                                       ShaderID tShaderID = ~0U);

private:
    std::unique_ptr<PplCacheBlockManager> _cacheBlockManager = nullptr;
    nvvk::GraphicsPipelineCreator         _gfxPipelineCreator;
    std::vector<VkPipeline>               _pipelines;
};

} // namespace Play

#endif // PLAY_PIPELINE_CACHE_MANAGER_H