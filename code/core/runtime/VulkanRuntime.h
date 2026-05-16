#pragma once

#include "SdlWindow.h"

#include <nvvk/context.hpp>
#include <nvvk/descriptors.hpp>
#include <nvvk/swapchain.hpp>

namespace Play
{
class DescriptorSetCache;
class FrameBufferCache;
class PipelineCacheManager;
class RefCounted;
class RenderPassCache;
class ToneMappingControlComponent;
} // namespace Play

namespace Play::runtime
{

struct CommandPool
{
    void init(VkDevice device, uint32_t queueFamilyIndex = 0, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    void cleanup();
    VkCommandBuffer allocCommandBuffer();
    void reset();

    VkDevice                    device           = VK_NULL_HANDLE;
    VkCommandPool               vkHandle         = VK_NULL_HANDLE;
    VkCommandBufferLevel        level            = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    std::vector<VkCommandBuffer> cmdBuffers       = {};
    uint32_t                    currCmdBufferIdx = 0;
};

template <int N = MAX_SUB_RENDER_THREAD>
class WorkerCommandContext
{
public:
    void init(VkDevice device, uint32_t queueFamilyIndex = 0, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_SECONDARY, uint32_t threadCount = N)
    {
        _pools.resize(threadCount);
        for (auto& pool : _pools)
        {
            pool.init(device, queueFamilyIndex, level);
        }
    }

    void cleanup()
    {
        for (auto& pool : _pools)
        {
            pool.cleanup();
        }
        _pools.clear();
    }

    void reset()
    {
        for (auto& pool : _pools)
        {
            pool.reset();
        }
    }

    VkCommandBuffer getCommandBuffer(uint32_t threadIndex, void* inheritanceInfo = nullptr)
    {
        if (threadIndex >= _pools.size())
        {
            return VK_NULL_HANDLE;
        }
        return _pools[threadIndex].allocCommandBuffer();
    }

    size_t getPoolCount() const
    {
        return _pools.size();
    }

private:
    std::vector<CommandPool> _pools;
};

class VulkanRuntime
{
public:
    struct FrameData
    {
        void init(VulkanRuntime& runtime, uint32_t graphicsQueueFamilyIndex, uint32_t computeQueueFamilyIndex);
        void deinit(VkDevice device);
        void reset();

        CommandPool            presentCmdPool;
        VkCommandBuffer        presentCmdBuffer = VK_NULL_HANDLE;
        CommandPool            graphicsCmdPool;
        CommandPool            computeCmdPool;
        WorkerCommandContext<> workerGraphicsPools;
        VkSemaphore            semaphore     = VK_NULL_HANDLE;
        uint64_t               timelineValue = 0;
    };

    VulkanRuntime(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo);
    ~VulkanRuntime();

    VulkanRuntime(const VulkanRuntime&)            = delete;
    VulkanRuntime& operator=(const VulkanRuntime&) = delete;
    VulkanRuntime(VulkanRuntime&&)                 = delete;
    VulkanRuntime& operator=(VulkanRuntime&&)      = delete;

    bool isInitialized() const
    {
        return _initialized;
    }

    void run();

    VkDevice getDevice() const
    {
        return _context.getDevice();
    }

    VkPhysicalDevice getPhysicalDevice() const
    {
        return _context.getPhysicalDevice();
    }

    VkInstance getInstance() const
    {
        return _context.getInstance();
    }

    const nvvk::QueueInfo& getQueue(uint32_t index) const
    {
        return _context.getQueueInfo(index);
    }

    const nvvk::QueueInfo& getGfxQueue() const
    {
        return _context.getQueueInfo(0);
    }

    const nvvk::QueueInfo& getComputeQueue() const
    {
        return _context.getQueueInfo(_context.getQueueInfos().size() > 1 ? 1 : 0);
    }

    const nvvk::QueueInfo& getTransferQueue() const
    {
        return _context.getQueueInfo(_context.getQueueInfos().size() > 2 ? 2 : 0);
    }

    bool isAsyncComputeQueueAvailable() const
    {
        return _context.getQueueInfos().size() > 1 && _context.getQueueInfo(1).queue != VK_NULL_HANDLE;
    }

    bool isAsyncQueue() const
    {
        return isAsyncComputeQueueAvailable();
    }

    VkCommandPool getCommandPool() const
    {
        return _transientCmdPool;
    }

    VkDescriptorPool getTextureDescriptorPool() const
    {
        return VK_NULL_HANDLE;
    }

    const VkExtent2D& getViewportSize() const
    {
        return _windowSize;
    }

    const VkExtent2D& getWindowSize() const
    {
        return _windowSize;
    }

    uint32_t getFrameCycleIndex() const
    {
        return _frameIndex;
    }

    uint32_t getFrameCycleSize() const
    {
        return static_cast<uint32_t>(_frames.size());
    }

    FrameData& getCurrentFrameData()
    {
        return _frames[_frameIndex];
    }

    Play::DescriptorSetCache* getDescriptorSetCache()
    {
        return _descriptorSetCache;
    }

    Play::PipelineCacheManager* getPipelineCacheManager()
    {
        return _pipelineCacheManager;
    }

    Play::ToneMappingControlComponent& getTonemapperControlComponent()
    {
        return *_tonemapperControlComponent;
    }

    VkCommandBuffer createTempCmdBuffer();
    void            submitAndWaitTempCmdBuffer(VkCommandBuffer cmd);
    void            addWaitSemaphore(const VkSemaphoreSubmitInfo& signalInfo);

    void deferDestroy(std::function<void()> task);
    void registerObject(Play::RefCounted* obj);
    void unregisterObject(Play::RefCounted* obj);
    void tryCleanupDeferredTasks();
    void tick();

    void updateFrameDescriptorSet();

    VkPhysicalDeviceProperties2 _physicalDeviceProperties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    bool                        _enableRayTracing        = true;
    bool                        _enableDynamicRendering  = true;
    Play::PipelineCacheManager* _pipelineCacheManager    = nullptr;

private:

    struct FrameStats
    {
        uint64_t lastTitleUpdateTicks        = 0;
        uint32_t framesSinceLastTitleUpdate  = 0;
        bool     titleUpdateTimerInitialized = false;
    };

    bool init(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo);
    void destroy();
    bool initContext(const nvvk::ContextInitInfo& contextInfo);
    bool initSurfaceAndSwapchain();
    bool initRenderServices();
    void deinitRenderServices();
    bool createTransientCommandPool();
    bool createFrameSubmission(uint32_t frameCount);
    void destroyFrameSubmission();
    void destroyDeferredTasks();

    void waitForCurrentFrame() const;
    bool rebuildSwapchain();
    bool prepareFrame();
    VkCommandBuffer beginCommandRecording();
    void recordBootstrapClear(VkCommandBuffer cmd) const;
    void endFrame(VkCommandBuffer cmd);
    void updateFrameStatsTitle();
    void prepareGlobalDescriptorSet();
    void updateGlobalDescriptorSet();
    void prepareFrameDescriptorSet();
    void presentFrame();

    struct DeferredDestroyQueue
    {
        std::vector<std::function<void()>> tasks;
    };

    RuntimeConfig                         _config{};
    SdlWindow                             _window{};
    nvvk::Context                         _context{};
    nvvk::Swapchain                       _swapchain{};
    VkSurfaceKHR                          _surface          = VK_NULL_HANDLE;
    VkCommandPool                         _transientCmdPool = VK_NULL_HANDLE;
    std::vector<FrameData>                _frames{};
    std::vector<DeferredDestroyQueue>     _deferredDestroyQueues{};
    std::vector<Play::RefCounted*>        _registeredObjects{};
    std::vector<VkSemaphoreSubmitInfo>    _pendingFrameWaitSemaphores{};
    nvvk::DescriptorBindings              _globalDescriptorBindings{};
    nvvk::DescriptorBindings              _frameDescriptorBindings{};
    Play::DescriptorSetCache*             _descriptorSetCache        = nullptr;
    Play::RenderPassCache*                _renderPassCache           = nullptr;
    Play::FrameBufferCache*               _frameBufferCache          = nullptr;
    Play::ToneMappingControlComponent*    _tonemapperControlComponent = nullptr;
    FrameStats                            _frameStats{};
    uint32_t                              _frameIndex   = 0;
    uint64_t                              _frameCounter = 0;
    uint64_t                              _lastTick     = 0;
    double                                _deltaTime    = 0.0;
    VkExtent2D                            _windowSize   = {};
    bool                                  _registeredAsGlobal = false;
    bool                                  _initialized  = false;
};

} // namespace Play::runtime

namespace Play
{
extern runtime::VulkanRuntime* vkDriver;
runtime::VulkanRuntime* GetVulkanRuntime();
} // namespace Play
