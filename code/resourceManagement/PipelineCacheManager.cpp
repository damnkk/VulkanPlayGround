#include "PipelineCacheManager.h"
#include <nvutils/file_mapping.hpp>
#include <nvutils/file_operations.hpp>
#include <nvutils/hash_operations.hpp>
#include "nvutils/parallel_work.hpp"
#include <nvvk/check_error.hpp>
#include <list>
#include <unordered_set>
#include "VulkanDriver.h"
namespace Play
{

using BlockKey = std::uint32_t;
struct CacheLRUNode
{
    BlockKey key;
};
using CacheLRU                      = std::list<CacheLRUNode>;
std::filesystem::path CacheBasePath = "./PipelineCache";

const uint32_t        MAX_PIPELINECACHE_SIZE_MB         = 256;
const uint32_t        MAX_RUNTIME_PIPELINECACHE_SIZE_MB = 64;
std::atomic<uint32_t> TotalCacheSize                    = 0;
PlayElement*          RHIContext                        = nullptr;

class CacheFileManager
{
public:
    static CacheFileManager& instance()
    {
        static CacheFileManager cacheFileManager;
        return cacheFileManager;
    }

    void deinit() {}
};

class PplCacheBlock
{
public:
    PplCacheBlock(BlockKey key) : _blockKey(key) {}
    bool loadFromDisk(std::filesystem::path filePath)
    {
        return false;
    }
    void savetoDisk(std::filesystem::path filePath) {}
    void setLRUNode(CacheLRUNode* node)
    {
        _lruNode = node;
    }

private:
    std::filesystem::path getCacheFilePath()
    {
        return CacheBasePath / (std::to_string(_blockKey) + ".bin");
    }
    enum class BlockState : uint8_t
    {
        eInitialized,
        eBuilding,
        eClosing,
        eFinalized,
        eFinalizedWithEvicted,
        eCount
    } _state                                 = BlockState::eCount;
    CacheLRUNode*                   _lruNode = nullptr;
    BlockKey                        _blockKey;
    std::unordered_set<PipelineKey> _pipelineKeys;
    VkPipelineCache                 _pipelineCache = VK_NULL_HANDLE;
    std::mutex                      _stateLock;
    std::mutex                      _pipelineCacheLock;
    std::atomic<uint32_t>           _uniquePipelineCount = 0;
    std::atomic<uint64_t>           _lastUsedFrame       = 0;
    std::atomic<uint32_t>           _cacheSize           = 0;
};

class PplCacheBlockManager
{
public:
    static PplCacheBlockManager& instance()
    {
        static PplCacheBlockManager pipelineBlockManager;
        return pipelineBlockManager;
    }

    PplCacheBlockManager()
    {
        loadAllBlockFromDisk();
    }

    void deinit() {}

    void createComputePipeline(ComputePipelineStateWithKey cState, std::function<void(VkPipelineCache)> createCPipelineFunc)
    {
        return;
    }

    void Tick() {}

    void tryToUnloadBlock() {}

private:
    std::filesystem::path getRootInfoPath()
    {
        return CacheBasePath / "rootInfo.bin";
    }
    struct HeaderInfo
    {
        uint32_t deviceID;
        uint32_t vendorID;
        uint8_t  pipelineCacheUUID[VK_UUID_SIZE];
        uint32_t blockCnt         = 0;
        uint32_t blockOffset      = 0;
        uint32_t CacheTotalSizeMB = 0;
    };

    void loadAllBlockFromDisk()
    {
        if (!std::filesystem::exists(getRootInfoPath()))
        {
            // 没有任何cache,则创建空文件,写入头信息就结束

            std::filesystem::create_directories(CacheBasePath);
            VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            vkGetPhysicalDeviceProperties2(vkDriver->_physicalDevice, &prop2);
            nvutils::FileReadOverWriteMapping mapping;
            mapping.open(getRootInfoPath(), sizeof(HeaderInfo));

            HeaderInfo header = {.deviceID = prop2.properties.deviceID, .vendorID = prop2.properties.vendorID};
            memcpy(header.pipelineCacheUUID, prop2.properties.pipelineCacheUUID, VK_UUID_SIZE);
            memcpy(mapping.data(), &header, sizeof(header));
            mapping.close();
        }
        else
        {
            // 首先验证头信息,然后判断cache是否过多,执行一定的删除逻辑
            VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            vkGetPhysicalDeviceProperties2(vkDriver->_physicalDevice, &prop2);
            nvutils::FileReadOverWriteMapping mapping;
            mapping.open(getRootInfoPath(), sizeof(HeaderInfo));
            HeaderInfo header;
            memcpy(&header, mapping.data(), sizeof(header));
            if (prop2.properties.deviceID != header.deviceID || prop2.properties.vendorID != header.vendorID ||
                memcmp(prop2.properties.pipelineCacheUUID, header.pipelineCacheUUID, VK_UUID_SIZE) != 0)
            {
                // 设备信息不匹配,删除所有cache文件
                mapping.close();
                std::filesystem::remove_all(CacheBasePath);
                std::filesystem::create_directories(CacheBasePath);
                loadAllBlockFromDisk(); // 重新创建新的文件
            }
            else
            {
                auto forPerBlock = [&](uint64_t index)
                {
                    BlockKey currBlockKey = static_cast<BlockKey>(index + header.blockOffset);
                    auto     block        = std::make_shared<PplCacheBlock>(currBlockKey);
                    if (!block->loadFromDisk(CacheBasePath / (std::to_string(currBlockKey) + ".bin")))
                    {
                        return;
                    }
                    auto node = std::make_shared<CacheLRUNode>(currBlockKey);
                    block->setLRUNode(node.get());
                    std::unique_lock<std::mutex> lk(_lock);
                    _blockMap[currBlockKey] = block;
                    _lru.push_front(*node);
                };
                nvutils::parallel_batches<8>(header.blockCnt, forPerBlock);
            }
        }
    }
    PplCacheBlock* getOrCreateBlock(PipelineKey key)
    {
        return nullptr;
    }
    CacheLRU                                                        _lru;
    std::unordered_map<PipelineKey, std::shared_ptr<PplCacheBlock>> _blockMap;
    std::mutex                                                      _lock;
    uint32_t                                                        _nextBlockKey = 0;
};

PipelineCacheManager::PipelineCacheManager() {}

void PipelineCacheManager::getOrCreateComputePipeline(const ComputePipelineStateWithKey&   cState,
                                                      std::function<void(VkPipelineCache)> createCPipelineFunc)
{
    PplCacheBlockManager::instance().createComputePipeline(cState, createCPipelineFunc);
}

PipelineCacheManager& PipelineCacheManager::Instance()
{
    static PipelineCacheManager instance;
    return instance;
}
void PipelineCacheManager::init() {}
void PipelineCacheManager::deinit() {}

} // namespace Play