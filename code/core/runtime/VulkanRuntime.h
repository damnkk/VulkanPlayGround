#pragma once

#include "SdlWindow.h"

#include <nvvk/context.hpp>
#include <nvvk/swapchain.hpp>

namespace Play::runtime
{

class VulkanRuntime
{
public:
    bool init(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo);
    void run();
    void deinit();

private:
    struct FrameData
    {
        VkCommandPool   cmdPool       = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer     = VK_NULL_HANDLE;
        VkSemaphore     timeline      = VK_NULL_HANDLE;
        uint64_t        timelineValue = 0;
    };

    struct FrameStats
    {
        uint64_t lastTitleUpdateTicks        = 0;
        uint32_t framesSinceLastTitleUpdate  = 0;
        bool     titleUpdateTimerInitialized = false;
    };

    bool initContext(const nvvk::ContextInitInfo& contextInfo);
    bool initSurfaceAndSwapchain();
    bool createTransientCommandPool();
    bool createFrameSubmission(uint32_t frameCount);
    void destroyFrameSubmission();

    void waitForCurrentFrame() const;
    bool rebuildSwapchain();
    bool prepareFrame();
    VkCommandBuffer beginCommandRecording();
    void recordBootstrapClear(VkCommandBuffer cmd) const;
    void endFrame(VkCommandBuffer cmd);
    void updateFrameStatsTitle();
    void presentFrame();

    RuntimeConfig          _config{};
    SdlWindow              _window{};
    nvvk::Context          _context{};
    nvvk::Swapchain        _swapchain{};
    VkSurfaceKHR           _surface          = VK_NULL_HANDLE;
    VkCommandPool          _transientCmdPool = VK_NULL_HANDLE;
    std::vector<FrameData> _frames{};
    FrameStats             _frameStats{};
    uint32_t               _frameIndex   = 0;
    uint64_t               _frameCounter = 0;
    VkExtent2D             _windowSize   = {};
    bool                   _initialized  = false;
};

} // namespace Play::runtime
