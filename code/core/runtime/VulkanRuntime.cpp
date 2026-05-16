#include "VulkanRuntime.h"

#include <SDL3/SDL_vulkan.h>
#include <nvutils/logger.hpp>
#include <nvvk/barriers.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/debug_util.hpp>

namespace Play::runtime
{

bool VulkanRuntime::init(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo)
{
    _config     = config;
    _frameStats = {};

    if (!_window.init(_config))
    {
        return false;
    }

    if (!initContext(contextInfo))
    {
        deinit();
        return false;
    }

    if (!_window.createSurface(_context.getInstance(), &_surface))
    {
        deinit();
        return false;
    }
    NVVK_DBG_NAME(_surface);

    if (!createTransientCommandPool())
    {
        deinit();
        return false;
    }

    if (!initSurfaceAndSwapchain())
    {
        deinit();
        return false;
    }

    _initialized = true;
    return true;
}

void VulkanRuntime::run()
{
    LOGI("Running SDL3 bootstrap runtime\n");

    while (!_window.shouldClose())
    {
        _window.pollEvents();

        if (!_window.isRenderable())
        {
            SDL_Delay(10);
            continue;
        }

        if (!prepareFrame())
        {
            continue;
        }

        VkCommandBuffer cmd = beginCommandRecording();
        recordBootstrapClear(cmd);
        endFrame(cmd);
        presentFrame();
    }

    if (_context.getDevice())
    {
        vkDeviceWaitIdle(_context.getDevice());
    }
}

void VulkanRuntime::deinit()
{
    if (_context.getDevice())
    {
        vkDeviceWaitIdle(_context.getDevice());
    }

    destroyFrameSubmission();

    if (_context.getDevice())
    {
        _swapchain.deinit();
    }

    if (_transientCmdPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(_context.getDevice(), _transientCmdPool, nullptr);
        _transientCmdPool = VK_NULL_HANDLE;
    }

    if (_surface != VK_NULL_HANDLE)
    {
        SDL_Vulkan_DestroySurface(_context.getInstance(), _surface, nullptr);
        _surface = VK_NULL_HANDLE;
    }

    if (_context.getInstance())
    {
        _context.deinit();
    }

    _window.deinit();
    _initialized = false;
}

bool VulkanRuntime::initContext(const nvvk::ContextInitInfo& contextInfo)
{
    nvvk::ContextInitInfo info = contextInfo;

    uint32_t                 sdlExtensionCount = 0;
    const char* const* const sdlExtensions     = _window.getVulkanInstanceExtensions(&sdlExtensionCount);
    if (!sdlExtensions || sdlExtensionCount == 0)
    {
        LOGE("SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
        return false;
    }

    for (uint32_t i = 0; i < sdlExtensionCount; ++i)
    {
        info.instanceExtensions.push_back(sdlExtensions[i]);
    }
    info.instanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

    const auto existingPreSelect = info.preSelectPhysicalDeviceCallback;
    info.preSelectPhysicalDeviceCallback =
        [existingPreSelect](VkInstance instance, VkPhysicalDevice physicalDevice)
    {
        if (existingPreSelect && !(*existingPreSelect)(instance, physicalDevice))
        {
            return false;
        }

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                SDL_Vulkan_GetPresentationSupport(instance, physicalDevice, i))
            {
                return true;
            }
        }

        return false;
    };

    if (_context.init(info) != VK_SUCCESS)
    {
        LOGE("nvvk::Context initialization failed\n");
        return false;
    }

    return true;
}

bool VulkanRuntime::initSurfaceAndSwapchain()
{
    nvvk::Swapchain::InitInfo swapchainInfo{
        .physicalDevice          = _context.getPhysicalDevice(),
        .device                  = _context.getDevice(),
        .queue                   = _context.getQueueInfo(0),
        .surface                 = _surface,
        .cmdPool                 = _transientCmdPool,
        .preferredVsyncOffMode   = VK_PRESENT_MODE_IMMEDIATE_KHR,
        .preferredVsyncOnMode    = VK_PRESENT_MODE_FIFO_KHR,
        .preferredImageCount     = 3,
        .preferredFramesInFlight = 3,
    };

    if (_swapchain.init(swapchainInfo) != VK_SUCCESS)
    {
        LOGE("Swapchain initialization failed\n");
        return false;
    }

    _windowSize = _window.getPixelSize();
    if (_swapchain.initResources(_windowSize, _config.vSync) != VK_SUCCESS)
    {
        LOGE("Swapchain resource initialization failed\n");
        return false;
    }

    return createFrameSubmission(_swapchain.getFramesInFlight());
}

bool VulkanRuntime::createTransientCommandPool()
{
    const VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = _context.getQueueInfo(0).familyIndex,
    };

    if (vkCreateCommandPool(_context.getDevice(), &poolInfo, nullptr, &_transientCmdPool) != VK_SUCCESS)
    {
        LOGE("Failed to create transient command pool\n");
        return false;
    }
    NVVK_DBG_NAME(_transientCmdPool);
    return true;
}

bool VulkanRuntime::createFrameSubmission(uint32_t frameCount)
{
    destroyFrameSubmission();

    _frames.resize(frameCount);
    _frameIndex = 0;

    const VkSemaphoreTypeCreateInfo timelineInfo{
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue  = 0,
    };
    const VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineInfo,
    };

    const VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = _context.getQueueInfo(0).familyIndex,
    };

    for (FrameData& frame : _frames)
    {
        if (vkCreateSemaphore(_context.getDevice(), &semaphoreInfo, nullptr, &frame.timeline) != VK_SUCCESS)
        {
            LOGE("Failed to create frame timeline semaphore\n");
            return false;
        }
        NVVK_DBG_NAME(frame.timeline);

        if (vkCreateCommandPool(_context.getDevice(), &poolInfo, nullptr, &frame.cmdPool) != VK_SUCCESS)
        {
            LOGE("Failed to create frame command pool\n");
            return false;
        }
        NVVK_DBG_NAME(frame.cmdPool);

        const VkCommandBufferAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = frame.cmdPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        if (vkAllocateCommandBuffers(_context.getDevice(), &allocInfo, &frame.cmdBuffer) != VK_SUCCESS)
        {
            LOGE("Failed to allocate frame command buffer\n");
            return false;
        }
        NVVK_DBG_NAME(frame.cmdBuffer);
    }

    return true;
}

void VulkanRuntime::destroyFrameSubmission()
{
    if (!_frames.empty() && _context.getDevice())
    {
        for (FrameData& frame : _frames)
        {
            if (frame.cmdBuffer != VK_NULL_HANDLE)
            {
                vkFreeCommandBuffers(_context.getDevice(), frame.cmdPool, 1, &frame.cmdBuffer);
                frame.cmdBuffer = VK_NULL_HANDLE;
            }
            if (frame.cmdPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(_context.getDevice(), frame.cmdPool, nullptr);
                frame.cmdPool = VK_NULL_HANDLE;
            }
            if (frame.timeline != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(_context.getDevice(), frame.timeline, nullptr);
                frame.timeline = VK_NULL_HANDLE;
            }
        }
        _frames.clear();
    }
}

void VulkanRuntime::waitForCurrentFrame() const
{
    const FrameData& frame = _frames[_frameIndex];
    if (frame.timelineValue == 0)
    {
        return;
    }

    const VkSemaphoreWaitInfo waitInfo{
        .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1,
        .pSemaphores    = &frame.timeline,
        .pValues        = &frame.timelineValue,
    };
    vkWaitSemaphores(_context.getDevice(), &waitInfo, UINT64_MAX);
}

bool VulkanRuntime::rebuildSwapchain()
{
    _windowSize = _window.getPixelSize();
    if (_windowSize.width == 0 || _windowSize.height == 0)
    {
        return false;
    }

    if (_swapchain.reinitResources(_windowSize, _config.vSync) != VK_SUCCESS)
    {
        LOGE("Swapchain rebuild failed\n");
        return false;
    }

    if (_swapchain.getFramesInFlight() != _frames.size())
    {
        return createFrameSubmission(_swapchain.getFramesInFlight());
    }

    _frameIndex = 0;
    return true;
}

bool VulkanRuntime::prepareFrame()
{
    if (_window.consumeResizePending())
    {
        _swapchain.requestRebuild();
    }

    if (_swapchain.needRebuilding() && !rebuildSwapchain())
    {
        return false;
    }

    waitForCurrentFrame();

    const VkResult result = _swapchain.acquireNextImage(_context.getDevice());
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        LOGE("Swapchain image acquire failed: %d\n", result);
        return false;
    }

    return true;
}

VkCommandBuffer VulkanRuntime::beginCommandRecording()
{
    FrameData& frame = _frames[_frameIndex];
    vkResetCommandPool(_context.getDevice(), frame.cmdPool, 0);

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    NVVK_CHECK(vkBeginCommandBuffer(frame.cmdBuffer, &beginInfo));
    return frame.cmdBuffer;
}

void VulkanRuntime::recordBootstrapClear(VkCommandBuffer cmd) const
{
    const float pulse = static_cast<float>(_frameCounter % 240) / 239.0F;
    const VkRenderingAttachmentInfo colorAttachment{
        .sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView   = _swapchain.getImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue  = {{{0.05F + 0.25F * pulse, 0.08F, 0.18F + 0.25F * (1.0F - pulse), 1.0F}}},
    };

    const VkRenderingInfo renderingInfo{
        .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea           = {{0, 0}, _windowSize},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachment,
    };

    nvvk::cmdImageMemoryBarrier(cmd, {_swapchain.getImage(), VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    vkCmdBeginRendering(cmd, &renderingInfo);
    vkCmdEndRendering(cmd);
    nvvk::cmdImageMemoryBarrier(cmd, {_swapchain.getImage(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR});
}

void VulkanRuntime::endFrame(VkCommandBuffer cmd)
{
    NVVK_CHECK(vkEndCommandBuffer(cmd));

    FrameData& frame = _frames[_frameIndex];

    const uint64_t signalValue = frame.timelineValue + 1;
    frame.timelineValue       = signalValue;

    const VkCommandBufferSubmitInfo cmdInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };

    const VkSemaphoreSubmitInfo waitInfo{
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = _swapchain.getAcquireSemaphore(),
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    const VkSemaphoreSubmitInfo signalInfos[2] = {
        {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = _swapchain.getPresentSemaphore(),
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        },
        {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = frame.timeline,
            .value     = signalValue,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
    };

    const VkSubmitInfo2 submitInfo{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = 1,
        .pWaitSemaphoreInfos      = &waitInfo,
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmdInfo,
        .signalSemaphoreInfoCount = 2,
        .pSignalSemaphoreInfos    = signalInfos,
    };

    NVVK_CHECK(vkQueueSubmit2(_context.getQueueInfo(0).queue, 1, &submitInfo, nullptr));
}

void VulkanRuntime::updateFrameStatsTitle()
{
    const uint64_t currentTicks = SDL_GetTicks();
    if (!_frameStats.titleUpdateTimerInitialized)
    {
        _frameStats.lastTitleUpdateTicks        = currentTicks;
        _frameStats.titleUpdateTimerInitialized = true;
        return;
    }

    ++_frameStats.framesSinceLastTitleUpdate;

    const uint64_t elapsedTicks = currentTicks - _frameStats.lastTitleUpdateTicks;
    if (elapsedTicks < 1000)
    {
        return;
    }

    const double fps     = static_cast<double>(_frameStats.framesSinceLastTitleUpdate) * 1000.0 / static_cast<double>(elapsedTicks);
    const double frameMs = static_cast<double>(elapsedTicks) / static_cast<double>(_frameStats.framesSinceLastTitleUpdate);

    char title[128]{};
    SDL_snprintf(title, sizeof(title), "%s - %.1f FPS (%.2f ms)", _config.windowTitle, fps, frameMs);
    _window.setTitle(title);

    _frameStats.framesSinceLastTitleUpdate = 0;
    _frameStats.lastTitleUpdateTicks       = currentTicks;
}

void VulkanRuntime::presentFrame()
{
    _swapchain.presentFrame(_context.getQueueInfo(0).queue);
    updateFrameStatsTitle();

    _frameIndex = (_frameIndex + 1) % static_cast<uint32_t>(_frames.size());
    ++_frameCounter;
}

} // namespace Play::runtime
