#include "PipelineCacheManager.h"
#include <nvutils/hash_operations.hpp>
#include <nvutils/parallel_work.hpp>
#include <nvvk/check_error.hpp>
#include "PlayProgram.h"
#include <list>
#include <unordered_set>
#include "VulkanDriver.h"
namespace Play
{
DataWriter* sqliteWriter = nullptr;

bool PplCacheBlock::loadFromDisk(bool fullLoading)
{
    BufferStream res{};
    if (!sqliteWriter->read(getBlockPath(), res))
    {
        return false;
    }
    res.read(&_blockKey, sizeof(BlockKey));
    res.read(&_currentPipelineCount, sizeof(uint32_t));
    _pipelineKeys.resize(_currentPipelineCount);
    res.read(_pipelineKeys.data(), sizeof(uint64_t) * _currentPipelineCount);

    std::unique_lock<std::mutex> lock(_stateLock);
    _state = _currentPipelineCount < MAX_BLOCK_PIPELINE_COUNT ? BLOCK_STATE_BUILDING : BLOCK_STATE_FINALIZED;
    if (fullLoading)
    {
        std::vector<uint8_t> cacheData;
        res.read(&_currPsoCacheSize, sizeof(size_t));
        cacheData.resize(_currPsoCacheSize);
        res.read(cacheData.data(), _currPsoCacheSize);
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        pipelineCacheCreateInfo.initialDataSize = _currPsoCacheSize;
        pipelineCacheCreateInfo.pInitialData    = cacheData.data();
        NVVK_CHECK(vkCreatePipelineCache(vkDriver->getDevice(), &pipelineCacheCreateInfo, nullptr, &_vkHandle));
        {
            _state &= ~BLOCK_STATE_EVICTED;
        }
    }
    return true;
}

void PplCacheBlock::saveToDisk()
{
    BufferStream stream;
    stream.write(&_blockKey, sizeof(BlockKey));
    stream.write(&_currentPipelineCount, sizeof(uint32_t));
    stream.write(_pipelineKeys.data(), sizeof(uint64_t) * _currentPipelineCount);
    stream.write(&_currPsoCacheSize, sizeof(size_t));
    vkGetPipelineCacheData(vkDriver->getDevice(), _vkHandle, reinterpret_cast<size_t*>(&_currPsoCacheSize), nullptr);
    std::vector<uint8_t> cacheData(_currPsoCacheSize);
    vkGetPipelineCacheData(vkDriver->getDevice(), _vkHandle, reinterpret_cast<size_t*>(&_currPsoCacheSize), cacheData.data());
    stream.write(cacheData.data(), _currPsoCacheSize);
}

void PplCacheBlock::init()
{
    _state = BLOCK_STATE_BUILDING;
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    pipelineCacheCreateInfo.initialDataSize = 0;
    pipelineCacheCreateInfo.pInitialData    = nullptr;
    NVVK_CHECK(vkCreatePipelineCache(vkDriver->getDevice(), &pipelineCacheCreateInfo, nullptr, &_vkHandle));
}

bool PplCacheBlock::tryAdd()
{
    std::unique_lock<std::mutex> lock(_stateLock);
    if (_state >= BLOCK_STATE_CLOSING)
    {
        return false;
    }
    ++_pendingPipelineCount;
    if (_currentPipelineCount + _pendingPipelineCount >= MAX_BLOCK_PIPELINE_COUNT)
    {
        _state = BLOCK_STATE_CLOSING;
    }
    return true;
}

void PplCacheBlock::createPipeline(std::function<void(VkPipelineCache)> createFunc)
{
    { // load data from disk if evicted
        std::unique_lock<std::mutex> lock(_stateLock);
        if (isEvicted())
        {
            loadFromDisk(true);
        }
    }
    { // create pipeline maybe modify the vkHandle, so need lock
        std::unique_lock<std::mutex> lock(_stateLock);
        createFunc(_vkHandle);
    }
    { // update state
        std::unique_lock<std::mutex> lock(_stateLock);
        ++_currentPipelineCount;
        --_pendingPipelineCount;
        if (_state == BLOCK_STATE_CLOSING && _pendingPipelineCount == 0)
        {
            _state = BLOCK_STATE_FINALIZED;
            saveToDisk();
        }
    }
}

PipelineKey PSOState::getPipelineKey()
{
    if (!dirtyFlag) return pipelineKey;
    PipelineKey key = 0;

    // 1. Hash Rasterization State
    nvutils::hashCombine(key, rasterizationState.depthClampEnable);
    nvutils::hashCombine(key, rasterizationState.rasterizerDiscardEnable);
    nvutils::hashCombine(key, rasterizationState.polygonMode);
    nvutils::hashCombine(key, rasterizationState.cullMode);
    nvutils::hashCombine(key, rasterizationState.frontFace);
    nvutils::hashCombine(key, rasterizationState.depthBiasEnable);
    // Note: depthBiasConstantFactor etc. are dynamic or float, usually not part of key if dynamic state is enabled
    // But if not dynamic, they should be hashed. Assuming dynamic for now or handled separately.
    nvutils::hashCombine(key, rasterizationState.lineWidth);

    // 2. Hash Multisample State
    nvutils::hashCombine(key, multisampleState.rasterizationSamples);
    nvutils::hashCombine(key, multisampleState.sampleShadingEnable);
    nvutils::hashCombine(key, multisampleState.minSampleShading);
    nvutils::hashCombine(key, multisampleState.alphaToCoverageEnable);
    nvutils::hashCombine(key, multisampleState.alphaToOneEnable);

    // 3. Hash Depth Stencil State
    nvutils::hashCombine(key, depthStencilState.depthTestEnable);
    nvutils::hashCombine(key, depthStencilState.depthWriteEnable);
    nvutils::hashCombine(key, depthStencilState.depthCompareOp);
    nvutils::hashCombine(key, depthStencilState.depthBoundsTestEnable);
    nvutils::hashCombine(key, depthStencilState.stencilTestEnable);
    // Hash Stencil Ops
    nvutils::hashCombine(key, depthStencilState.front.failOp);
    nvutils::hashCombine(key, depthStencilState.front.passOp);
    nvutils::hashCombine(key, depthStencilState.front.depthFailOp);
    nvutils::hashCombine(key, depthStencilState.front.compareOp);
    nvutils::hashCombine(key, depthStencilState.back.failOp);
    nvutils::hashCombine(key, depthStencilState.back.passOp);
    nvutils::hashCombine(key, depthStencilState.back.depthFailOp);
    nvutils::hashCombine(key, depthStencilState.back.compareOp);

    // 4. Hash Color Blend State
    nvutils::hashCombine(key, colorBlendState.logicOpEnable);
    nvutils::hashCombine(key, colorBlendState.logicOp);
    // Hash attachments blend state
    for (const auto& enable : colorBlendEnables)
    {
        nvutils::hashCombine(key, enable);
    }
    for (const auto& mask : colorWriteMasks)
    {
        nvutils::hashCombine(key, mask);
    }
    for (const auto& eq : colorBlendEquations)
    {
        nvutils::hashCombine(key, eq.srcColorBlendFactor);
        nvutils::hashCombine(key, eq.dstColorBlendFactor);
        nvutils::hashCombine(key, eq.colorBlendOp);
        nvutils::hashCombine(key, eq.srcAlphaBlendFactor);
        nvutils::hashCombine(key, eq.dstAlphaBlendFactor);
        nvutils::hashCombine(key, eq.alphaBlendOp);
    }

    // 5. Hash Input Assembly
    nvutils::hashCombine(key, inputAssemblyState.topology);
    nvutils::hashCombine(key, inputAssemblyState.primitiveRestartEnable);

    // 7. Hash Dynamic States List
    // The presence of a dynamic state affects the pipeline layout
    for (const auto& ds : dynamicStates)
    {
        nvutils::hashCombine(key, ds);
    }
    dirtyFlag = false;
    return key;
}

PplCacheBlockManager::PplCacheBlockManager()
{
    loadAllBlockFromDisk();
}

void PplCacheBlockManager::saveHeaderInfo()
{
    _HeaderInfo.blockCnt = static_cast<uint32_t>(_blockMap.size());
    _HeaderInfo.blockKeys.clear();
    for (auto& [first, second] : _blockMap)
    {
        _HeaderInfo.blockKeys.push_back(first);
    }
    BufferStream tempData;
    tempData.write(&_HeaderInfo.deviceID, sizeof(uint32_t));
    tempData.write(&_HeaderInfo.vendorID, sizeof(uint32_t));
    tempData.write(&_HeaderInfo.pipelineCacheUUID, VK_UUID_SIZE);
    tempData.write(&_HeaderInfo.blockCnt, sizeof(uint32_t));
    tempData.write(&_HeaderInfo.blockKeys);

    sqliteWriter->write(getRootInfoPath().string(), tempData);
}

void PplCacheBlockManager::HeaderInfo::initFromLoadRes(BufferStream& res)
{
    res.read(this, offsetof(HeaderInfo, pipelineCacheUUID));
    res.read(pipelineCacheUUID, VK_UUID_SIZE);
    blockKeys.resize(blockCnt);
    res.read(blockKeys.data(), sizeof(BlockKey) * blockCnt);
}

void PplCacheBlockManager::loadAllBlockFromDisk()
{
    if (!std::filesystem::exists(sqliteWriter->getRootPath() / PIPELINE_CACHE_PATH / PIPELINE_CACHE_FILE_NAME))
    {
        sqliteWriter->open((PIPELINE_CACHE_PATH / PIPELINE_CACHE_FILE_NAME).string());
        memcpy(_HeaderInfo.pipelineCacheUUID, vkDriver->_physicalDeviceProperties2.properties.pipelineCacheUUID, VK_UUID_SIZE);
        _HeaderInfo.deviceID = vkDriver->_physicalDeviceProperties2.properties.deviceID;
        _HeaderInfo.vendorID = vkDriver->_physicalDeviceProperties2.properties.vendorID;
        saveHeaderInfo();
    }
    else
    {
        // 首先验证头信息,然后判断cache是否过多,执行一定的删除逻辑
        VkPhysicalDeviceProperties2 prop2 = vkDriver->_physicalDeviceProperties2;
        BufferStream                res{};
        sqliteWriter->open((PIPELINE_CACHE_PATH / PIPELINE_CACHE_FILE_NAME).string());
        sqliteWriter->read(getRootInfoPath().string(), res);

        _HeaderInfo.initFromLoadRes(res);
        if (prop2.properties.deviceID != _HeaderInfo.deviceID || prop2.properties.vendorID != _HeaderInfo.vendorID ||
            memcmp(prop2.properties.pipelineCacheUUID, _HeaderInfo.pipelineCacheUUID, VK_UUID_SIZE) != 0)
        {
            // 设备信息不匹配,删除所有cache文件
            sqliteWriter->close();
            std::filesystem::remove(PIPELINE_CACHE_PATH / PIPELINE_CACHE_FILE_NAME);
            loadAllBlockFromDisk(); // 重新创建新的文件
        }
        else
        {
            auto forPerBlock = [&](uint64_t index)
            {
                BlockKey currBlockKey = static_cast<BlockKey>(index);
                auto     block        = std::make_unique<PplCacheBlock>(currBlockKey);
                if (!block->loadFromDisk())
                {
                    return;
                }

                std::unique_lock<std::mutex> lk(_lock);
                _blockMap[currBlockKey] = std::move(block);
                for (auto& pipelineKey : block->_pipelineKeys)
                {
                    _pipelineToBlockMap[pipelineKey] = currBlockKey;
                }
                _lru.push_front(currBlockKey);
                _nextBlockKey = std::max(_nextBlockKey, currBlockKey + 1);
            };
            for (size_t i = 0; i < _HeaderInfo.blockKeys.size(); ++i)
            {
                forPerBlock(i);
            }
        }
    }
}

PplCacheBlock* PplCacheBlockManager::getOrCreateBlock(PipelineKey key)
{
    // Check if the key already exists in the map
    if (_pipelineToBlockMap.find(key) != _pipelineToBlockMap.end())
    {
        // Found existing block
        BlockKey targetBlock = _pipelineToBlockMap[key];
        if (!_blockMap.contains(targetBlock))
        {
            _pipelineToBlockMap.erase(key);
            return getOrCreateBlock(key);
        }
        // if the block is not loaded, we need to load it
        {
            std::unique_lock<std::mutex> lock(_blockMap[targetBlock]->_stateLock);
            if (_blockMap[targetBlock]->isEvicted())
            {
                if (!_blockMap[targetBlock]->loadFromDisk(true))
                {
                    // if load failed, remove it from map
                    _blockMap.erase(targetBlock);
                    _pipelineToBlockMap.erase(key);
                    return getOrCreateBlock(key);
                }
            }
        }
        for (auto it = _lru.begin(); it != _lru.end(); ++it)
        {
            if (*it == targetBlock)
            {
                // Move this element to the front
                _lru.splice(_lru.begin(), _lru, it);
                break;
            }
        }
        return _blockMap[targetBlock].get();
    }
    // Check if current block can be used
    else if (_blockMap[_nextBlockKey]->tryAdd())
    {
        // 如果当前block可以添加,则添加到当前block
        _pipelineToBlockMap[key] = _nextBlockKey;
        if (_blockMap[_nextBlockKey]->isEvicted())
        {
            if (!_blockMap[_nextBlockKey]->loadFromDisk(true))
            {
                // if load failed, remove it from map
                _blockMap.erase(_nextBlockKey);
                _pipelineToBlockMap.erase(key);
                return getOrCreateBlock(key);
            }
        }
        // Move to head
        for (auto it = _lru.begin(); it != _lru.end(); ++it)
        {
            if (*it == _nextBlockKey)
            {
                _lru.splice(_lru.begin(), _lru, it);
                break;
            }
        }
        return _blockMap[_nextBlockKey].get();
    }
    // Create a new block
    else
    {
        // 如果没有可用的block,则创建一个新的block
        auto newBlock = std::make_unique<PplCacheBlock>(_nextBlockKey);
        newBlock->init();
        _blockMap[_nextBlockKey] = std::move(newBlock);
        _pipelineToBlockMap[key] = _nextBlockKey;
        _lru.push_front(_nextBlockKey);
        ++_nextBlockKey;
        _blockMap[_nextBlockKey - 1]->tryAdd();
        return _blockMap[_nextBlockKey - 1].get();
    }
}

PipelineCacheManager::PipelineCacheManager()
{
    sqliteWriter       = new DataWriter();
    _cacheBlockManager = std::make_unique<PplCacheBlockManager>();
}
PipelineCacheManager::~PipelineCacheManager()
{
    if (sqliteWriter)
    {
        sqliteWriter->close();
        delete sqliteWriter;
        sqliteWriter = nullptr;
    }
}

VkPipeline PipelineCacheManager::getOrCreateGraphicsPipeline(RenderProgram* program)
{
    uint64_t key = program->psoState().getPipelineKey();
    nvutils::hashCombine(key, program->getVertexModuleID());
    nvutils::hashCombine(key, program->getFragModuleID());
    auto vShaderModule = ShaderManager::Instance().getShaderById(program->getVertexModuleID());
    auto fShaderModule = ShaderManager::Instance().getShaderById(program->getFragModuleID());
    _gfxPipelineCreator.clearShaders();
    _gfxPipelineCreator.addShader(VK_SHADER_STAGE_VERTEX_BIT, vShaderModule->_entryPoint.c_str(), vShaderModule->_shaderModule);
    _gfxPipelineCreator.addShader(VK_SHADER_STAGE_FRAGMENT_BIT, fShaderModule->_entryPoint.c_str(), fShaderModule->_shaderModule);

    _gfxPipelineCreator.pipelineInfo.layout = program->getDescriptorSetManager().getPipelineLayout();

    if (vkDriver->_enableDynamicRendering)
    {
        DynamicRenderPass* dRenderPass                           = dynamic_cast<DynamicRenderPass*>(program->getRenderPass());
        _gfxPipelineCreator.renderingState.depthAttachmentFormat = dRenderPass->getDepthAttachmentFormat();
        _gfxPipelineCreator.colorFormats                         = dRenderPass->getColorAttachmentFormats();
    }
    else
    {
        LOGE("Not support general render pass");
    }

    _gfxPipelineCreator.createGraphicsPipeline(vkDriver->getDevice(), VK_NULL_HANDLE, program->psoState(), &_pipelineMap[key]);
    return _pipelineMap[key];
}
VkPipeline PipelineCacheManager::getOrCreateComputePipeline(ComputePipelineState& computeState)
{
    return VK_NULL_HANDLE;
}
VkPipeline PipelineCacheManager::getOrCreateRTPipeline(RTPipelineState& rtState)
{
    return VK_NULL_HANDLE;
}
VkPipeline PipelineCacheManager::getOrCreateMeshPipeline(PSOState& psoState, RenderPass* renderPass, ShaderID mShaderID, ShaderID fShaderID,
                                                         ShaderID tShaderID)
{
    return VK_NULL_HANDLE;
}

} // namespace Play