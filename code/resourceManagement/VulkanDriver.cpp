#include "VulkanDriver.h"
#include "DescriptorManager.h"
#include "RenderPassCache.h"
#include "FrameBufferCache.h"
#include "PipelineCacheManager.h"
#include <nvvk/check_error.hpp>
#include <nvvk/debug_util.hpp>
#include <stdexcept>
namespace Play
{

VulkanDriver* vkDriver = nullptr;

VulkanDriver::VulkanDriver(nvapp::Application* app) : _app(app)
{
    // 1. 缓存核心句柄
    _device         = app->getDevice();
    _physicalDevice = app->getPhysicalDevice();
    _instance       = app->getInstance();

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
    _frameData.resize(_app->getFrameCycleSize());
}

void VulkanDriver::init()
{
    PlayResourceManager::Instance().initialize();
    TexturePool::Instance().init(65535, &PlayResourceManager::Instance());
    BufferPool::Instance().init(65535, &PlayResourceManager::Instance());
    ShaderManager::Instance().init();
    PipelineCacheManager::Instance().init();
    if (!_enableDynamicRendering)
    {
        _renderPassCache  = std::make_unique<RenderPassCache>();
        _frameBufferCache = std::make_unique<FrameBufferCache>();
    }

    for (size_t i = 0; i < _frameData.size(); ++i)
    {
        VkCommandPoolCreateInfo cmdPoolCI{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolCI.queueFamilyIndex = _queueGraphics.familyIndex;
        NVVK_CHECK(vkCreateCommandPool(_device, &cmdPoolCI, nullptr, &_frameData[i].graphicsCmdPool));
        cmdPoolCI.queueFamilyIndex = _queueCompute.familyIndex;
        NVVK_CHECK(vkCreateCommandPool(_device, &cmdPoolCI, nullptr, &_frameData[i].computeCmdPool));
        VkSemaphoreTypeCreateInfo timelineCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue  = 0;

        VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        semaphoreCreateInfo.flags = 0;
        semaphoreCreateInfo.pNext = &timelineCreateInfo;

        NVVK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frameData[i].semaphore));
    }
}

VulkanDriver::~VulkanDriver()
{
    vkQueueWaitIdle(_app->getQueue(0).queue);
    for (auto& frame : _frameData)
    {
        vkDestroySemaphore(_device, frame.semaphore, nullptr);
        vkDestroyCommandPool(_device, frame.graphicsCmdPool, nullptr);
        vkDestroyCommandPool(_device, frame.computeCmdPool, nullptr);
    }
    _descriptorSetCache.reset();
    TexturePool::Instance().deinit();
    BufferPool::Instance().deinit();
    PlayResourceManager::Instance().deInit();
    ShaderManager::Instance().deInit();
    PipelineCacheManager::Instance().deinit();
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
    _frameNum++;
}

} // namespace Play