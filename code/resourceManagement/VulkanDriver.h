#pragma once

#include <volk.h>
#include <nvvk/context.hpp>
#include <nvapp/application.hpp>
#include <nvvk/descriptors.hpp>
#include <memory>
#include <cassert>
#include <queue>
#include "core/JobSystem.h"

namespace Play
{

class DescriptorSetCache;
class RenderPassCache;
class FrameBufferCache;
class PipelineCacheManager;
struct PlayFrameData
{
    VkCommandPool                graphicsCmdPool;
    VkCommandPool                computeCmdPool;
    std::vector<VkCommandBuffer> cmdBuffersGfx;
    std::vector<VkCommandBuffer> cmdBuffersCompute;
    VkSemaphore                  semaphore;
    uint64_t                     timelineValue = 0;
    void                         reset(VkDevice device)
    {
        if (!cmdBuffersGfx.empty())
        {
            vkFreeCommandBuffers(device, graphicsCmdPool, (uint32_t) cmdBuffersGfx.size(), cmdBuffersGfx.data());
            cmdBuffersGfx.clear();
        }
        if (!cmdBuffersCompute.empty())
        {
            vkFreeCommandBuffers(device, computeCmdPool, (uint32_t) cmdBuffersCompute.size(), cmdBuffersCompute.data());
            cmdBuffersCompute.clear();
        }
        vkResetCommandPool(device, graphicsCmdPool, 0);
        vkResetCommandPool(device, computeCmdPool, 0);
    }
};

/**
 * @brief VulkanDriver 是底层图形驱动的单例封装。
 * 它负责持有 VkDevice, VkQueue 等核心句柄，以及全局共享的缓存（如 DescriptorCache）。
 * 任何需要访问 Vulkan API 的地方都应该通过这个类，而不是 PlayApp。
 */

class VulkanDriver
{
public:
    VulkanDriver(nvapp::Application* app);
    ~VulkanDriver();
    void init();

public:
    // --- 核心 Vulkan 访问器 ---
    VkDevice getDevice() const
    {
        return _device;
    }
    VkPhysicalDevice getPhysicalDevice() const
    {
        return _physicalDevice;
    }
    VkInstance getInstance() const
    {
        return _instance;
    }

    const nvvk::QueueInfo& getGfxQueue() const
    {
        return _queueGraphics;
    }
    // 获取计算队列 (Async Compute)
    const nvvk::QueueInfo& getComputeQueue() const
    {
        return _queueCompute;
    }
    // 获取传输队列 (Transfer)
    const nvvk::QueueInfo& getTransferQueue() const
    {
        return _queueTransfer;
    }

    // 检查是否支持异步计算队列
    bool isAsyncComputeQueueAvailable() const
    {
        return _queueCompute.queue != VK_NULL_HANDLE;
    }

    nvapp::Application* getApp()
    {
        return _app;
    }
    inline VkDevice getDevice()
    {
        return _app->getDevice();
    }
    inline VkPhysicalDevice getPhysicalDevice()
    {
        return _app->getPhysicalDevice();
    }
    inline const nvvk::QueueInfo& getQueue(uint32_t index)
    {
        return _app->getQueue(index);
    }
    inline VkCommandPool getCommandPool() const
    {
        return _app->getCommandPool();
    }
    inline VkDescriptorPool getTextureDescriptorPool() const
    {
        return _app->getTextureDescriptorPool();
    }
    inline const VkExtent2D& getViewportSize() const
    {
        return _app->getViewportSize();
    }
    inline const VkExtent2D& getWindowSize() const
    {
        return _app->getWindowSize();
    }
    inline GLFWwindow* getWindowHandle() const
    {
        return _app->getWindowHandle();
    }
    inline uint32_t getFrameCycleIndex() const
    {
        return _app->getFrameCycleIndex();
    }
    inline uint32_t getFrameCycleSize() const
    {
        return _app->getFrameCycleSize();
    }

    inline PlayFrameData& getCurrentFrameData()
    {
        return _frameData[_app->getFrameCycleIndex()];
    }

    DescriptorSetCache* getDescriptorSetCache()
    {
        return _descriptorSetCache.get();
    }

    inline bool isAsyncQueue() const
    {
        try
        {
            return _app->getQueue(2).queue != VK_NULL_HANDLE;
        }
        catch (const std::out_of_range&)
        {
            return false;
        }
    }

    VkPhysicalDeviceProperties2 _physicalDeviceProperties2;

private:
    void prepareGlobalDescriptorSet();
    void updateGlobalDescriptorSet();
    void prepareFrameDescriptorSet();
    void updateFrameDescriptorSet();

    VkDescriptorSet          getGlobalDescriptorSet();
    VkDescriptorSetLayout    getGlobalDescriptorSetLayout();
    nvvk::DescriptorBindings _globalDescriptorBindings;
    nvvk::DescriptorBindings _frameDescriptorBindings;

public:
    void tryCleanupDeferredTasks();
    void tick();

    std::vector<PlayFrameData>            _frameData;
    std::unique_ptr<DescriptorSetCache>   _descriptorSetCache   = nullptr;
    std::unique_ptr<PipelineCacheManager> _pipelineCacheManager = nullptr;

    // 核心句柄 (从 Application 拷贝过来，避免每次都解引用 App)
    VkDevice         _device         = VK_NULL_HANDLE;
    VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
    VkInstance       _instance       = VK_NULL_HANDLE;

    // 队列信息
    nvvk::QueueInfo                                       _queueGraphics; // Graphics/Compute/Transfer (Main)
    nvvk::QueueInfo                                       _queueTransfer; // Dedicated Transfer
    nvvk::QueueInfo                                       _queueCompute;  // Dedicated Compute
    std::queue<std::pair<uint8_t, std::function<void()>>> _deferredDeleteTaskQueue;

    // 原始 App 指针 (仅用于获取窗口大小等非 Vulkan 核心信息，如果需要的话)
    nvapp::Application*               _app                    = nullptr;
    uint64_t                          _frameNum               = 0;
    bool                              _enableRayTracing       = true;
    bool                              _enableDynamicRendering = true;
    std::unique_ptr<RenderPassCache>  _renderPassCache        = nullptr; // if Dynamic rendering is off, use RenderPassCache to manage render passes
    std::unique_ptr<FrameBufferCache> _frameBufferCache       = nullptr; // if Dynamic rendering is off, use FrameBufferCache to manage frame buffers
};
extern VulkanDriver* vkDriver;

} // namespace Play