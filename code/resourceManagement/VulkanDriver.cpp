#include "VulkanDriver.h"
#include "DescriptorManager.h"
#include "RenderPassCache.h"
#include "FrameBufferCache.h"
#include "PipelineCacheManager.h"
#include "Material.h"
#include <nvvk/check_error.hpp>
#include <nvvk/debug_util.hpp>
#include <nvutils/parallel_work.hpp>
#include <stdexcept>
namespace Play
{

CommandPool::~CommandPool()
{
    vkDestroyCommandPool(vkDriver->getDevice(), vkHandle, nullptr);
    vkHandle = VK_NULL_HANDLE;
}

void CommandPool::init(uint32_t queueFamilyIndex, VkCommandBufferLevel level)
{
    this->level = level;
    VkCommandPoolCreateInfo cmdPoolCI{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cmdPoolCI.queueFamilyIndex = queueFamilyIndex;
    cmdPoolCI.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    NVVK_CHECK(vkCreateCommandPool(vkDriver->getDevice(), &cmdPoolCI, nullptr, &vkHandle));
}

VkCommandBuffer CommandPool::allocCommandBuffer()
{
    if (currCmdBufferIdx >= cmdBuffers.size())
    {
        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool        = vkHandle;
        allocInfo.level              = level;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmdBuffer;
        NVVK_CHECK(vkAllocateCommandBuffers(vkDriver->getDevice(), &allocInfo, &cmdBuffer));
        cmdBuffers.push_back(cmdBuffer);
    }
    return cmdBuffers[currCmdBufferIdx++];
}

void CommandPool::reset()
{
    std::for_each(cmdBuffers.begin(), cmdBuffers.end(), [](VkCommandBuffer cmdBuffer) { vkResetCommandBuffer(cmdBuffer, 0); });
    currCmdBufferIdx = 0;
}

PlayFrameData::PlayFrameData() : graphicsCmdPool(), computeCmdPool(1)
{
    graphicsCmdPool.init();
    computeCmdPool.init(1);
    workerGraphicsPools.init();
    VkSemaphoreTypeCreateInfo timelineCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue  = 0;

    VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    semaphoreCreateInfo.flags = 0;
    semaphoreCreateInfo.pNext = &timelineCreateInfo;

    NVVK_CHECK(vkCreateSemaphore(vkDriver->getDevice(), &semaphoreCreateInfo, nullptr, &semaphore));
}

PlayFrameData::~PlayFrameData()
{
    vkDestroySemaphore(vkDriver->getDevice(), semaphore, nullptr);
}

VulkanDriver* vkDriver = nullptr;

VulkanDriver::VulkanDriver(nvapp::Application* app) : _app(app)
{
    auto& threadPool = nvutils::get_thread_pool();

    // 1. 缓存核心句柄
    _device                    = app->getDevice();
    _physicalDevice            = app->getPhysicalDevice();
    _instance                  = app->getInstance();
    _physicalDeviceProperties2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(_physicalDevice, &_physicalDeviceProperties2);

    // 2. 缓存队列信息
    // 注意：这里假设 nvapp 的队列索引约定。通常 0 是 GCT，1 是 Transfer，2 是 Compute
    // 你需要根据你的 nvapp 配置确认这一点
    _queueGraphics = app->getQueue(0);

    // 尝试获取其他队列，如果不存在可能会抛出异常或返回空，这里做安全检查
    try
    {
        _queueTransfer = app->getQueue(1);
    }
    catch (...)
    {
        _queueTransfer = {~0u, ~0u, VK_NULL_HANDLE};
    }

    try
    {
        _queueCompute = app->getQueue(2);
    }
    catch (...)
    {
        _queueCompute = {~0u, ~0u, VK_NULL_HANDLE};
    }
    nvvk::DebugUtil::getInstance().init(_app->getDevice());

    _descriptorSetCache = std::make_unique<DescriptorSetCache>();

    _lastTickTime = std::chrono::high_resolution_clock::now();
}

void VulkanDriver::prepareGlobalDescriptorSet()
{
    _globalDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr);  // g_GlobalTexture
    _globalDescriptorBindings.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr);  // g_GlobalLutTexture  for tone map
    _globalDescriptorBindings.addBinding(2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr);        // g_GlobalSampler_Nerest
    _globalDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr);        // g_GlobalSampler_Linear
    _globalDescriptorBindings.addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr); // g_GlobalSampler_Linear
    _descriptorSetCache->initGlobalDescriptorSets(_globalDescriptorBindings);
    updateGlobalDescriptorSet();
}

void VulkanDriver::updateGlobalDescriptorSet()
{
    std::vector<VkSampler>             samplerList(2);
    std::vector<VkDescriptorImageInfo> imageInfoList;
    VkSamplerCreateInfo                samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.magFilter     = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter     = VK_FILTER_NEAREST;
    samplerCreateInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerCreateInfo.mipLodBias    = 0.0f;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    samplerCreateInfo.compareEnable = VK_FALSE;
    PlayResourceManager::Instance().acquireSampler(samplerList[0], samplerCreateInfo);
    imageInfoList.push_back({samplerList[0]});
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    PlayResourceManager::Instance().acquireSampler(samplerList[1], samplerCreateInfo);
    imageInfoList.push_back({samplerList[1]});
    VkDescriptorBufferInfo toneMappingBufferInfo{};
    toneMappingBufferInfo.buffer = _tonemapperControlComponent->getGPUBuffer()->buffer;
    toneMappingBufferInfo.offset = 0;
    toneMappingBufferInfo.range  = VK_WHOLE_SIZE;
    std::vector<VkWriteDescriptorSet> writeSet(3, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET});
    writeSet[0].dstSet          = _descriptorSetCache->getEngineDescriptorSet().set;
    writeSet[0].dstBinding      = 2;
    writeSet[0].descriptorCount = 1;
    writeSet[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    writeSet[0].pImageInfo      = &imageInfoList[0];
    writeSet[1].dstSet          = _descriptorSetCache->getEngineDescriptorSet().set;
    writeSet[1].dstBinding      = 3;
    writeSet[1].descriptorCount = 1;
    writeSet[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    writeSet[1].pImageInfo      = &imageInfoList[1];
    writeSet[2].dstSet          = _descriptorSetCache->getEngineDescriptorSet().set;
    writeSet[2].dstBinding      = 4;
    writeSet[2].descriptorCount = 1;
    writeSet[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeSet[2].pBufferInfo     = &toneMappingBufferInfo;

    vkUpdateDescriptorSets(vkDriver->_device, static_cast<uint32_t>(writeSet.size()), writeSet.data(), 0, nullptr);
}

void VulkanDriver::prepareFrameDescriptorSet()
{
    _frameDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr); // g_CameraDataBuffer
    _descriptorSetCache->initFrameDescriptorSets(_frameDescriptorBindings);
}

void            VulkanDriver::updateFrameDescriptorSet() {}
VkDescriptorSet VulkanDriver::getGlobalDescriptorSet()
{
    return VK_NULL_HANDLE;
}
VkDescriptorSetLayout VulkanDriver::getGlobalDescriptorSetLayout()
{
    return VK_NULL_HANDLE;
}

void VulkanDriver::init()
{
    _pipelineCacheManager = std::make_unique<PipelineCacheManager>();
    PlayResourceManager::Instance().initialize();
    TexturePool::Instance().init(65535, &PlayResourceManager::Instance());
    BufferPool::Instance().init(65535, &PlayResourceManager::Instance());
    ProgramPool::Instance().init(65535, &PlayResourceManager::Instance());
    ShaderManager::Instance().init();

    if (!_enableDynamicRendering)
    {
        _renderPassCache  = std::make_unique<RenderPassCache>();
        _frameBufferCache = std::make_unique<FrameBufferCache>();
    }
    _frameData.resize(_app->getFrameCycleSize());
    _tonemapperControlComponent = std::make_unique<ToneMappingControlComponent>();
    prepareGlobalDescriptorSet();
    prepareFrameDescriptorSet();
}

VulkanDriver::~VulkanDriver()
{
    vkQueueWaitIdle(_app->getQueue(0).queue);
    for (auto& frame : _frameData)
    {
    }
    _descriptorSetCache.reset();
    TexturePool::Instance().deinit();
    BufferPool::Instance().deinit();
    ProgramPool::Instance().deinit();
    PlayResourceManager::Instance().deInit();
    ShaderManager::Instance().deInit();
}

void VulkanDriver::tryCleanupDeferredTasks()
{
    while (!vkDriver->_deferredDeleteTaskQueue.empty())
    {
        auto& taskPair = vkDriver->_deferredDeleteTaskQueue.front();
        if ((uint32_t) taskPair.first != _app->getFrameCycleIndex()) break;
        taskPair.second();
        vkDriver->_deferredDeleteTaskQueue.pop();
    }
}

void VulkanDriver::tick()
{
    auto                          currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff        = currentTime - _lastTickTime;
    _deltaTime                                = diff.count();
    _lastTickTime                             = currentTime;

    _frameNum++;
}

} // namespace Play