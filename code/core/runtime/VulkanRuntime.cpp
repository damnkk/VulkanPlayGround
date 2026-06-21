#include "VulkanRuntime.h"

#include <SDL3/SDL_vulkan.h>
#include <nvutils/logger.hpp>
#include <nvvk/barriers.hpp>
#include <nvvk/check_error.hpp>
#include <nvvk/debug_util.hpp>

#include "DescriptorManager.h"
#include "FrameBufferCache.h"
#include "PipelineCacheManager.h"
#include "RenderSession.h"
#include "PlayAllocator.h"
#include "RenderPassCache.h"
#include "Resource.h"
#include "ShaderManager.hpp"
#include "core/RefCounted.h"

namespace Play
{
runtime::VulkanRuntime* vkDriver = nullptr;

runtime::VulkanRuntime* GetVulkanRuntime()
{
    return vkDriver;
}
} // namespace Play

namespace Play::runtime
{

void CommandPool::init(VkDevice inputDevice, uint32_t queueFamilyIndex, VkCommandBufferLevel inputLevel)
{
    device = inputDevice;
    level  = inputLevel;

    const VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    NVVK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &vkHandle));
}

void CommandPool::cleanup()
{
    if (vkHandle != VK_NULL_HANDLE && device != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device, vkHandle, nullptr);
    }

    vkHandle         = VK_NULL_HANDLE;
    device           = VK_NULL_HANDLE;
    currCmdBufferIdx = 0;
    cmdBuffers.clear();
}

VkCommandBuffer CommandPool::allocCommandBuffer()
{
    if (currCmdBufferIdx >= cmdBuffers.size())
    {
        const VkCommandBufferAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool        = vkHandle,
            .level              = level,
            .commandBufferCount = 1,
        };

        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
        NVVK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer));
        cmdBuffers.push_back(cmdBuffer);
    }

    return cmdBuffers[currCmdBufferIdx++];
}

void CommandPool::reset()
{
    if (vkHandle != VK_NULL_HANDLE)
    {
        vkResetCommandPool(device, vkHandle, 0);
    }
    currCmdBufferIdx = 0;
}

void VulkanRuntime::FrameData::init(VulkanRuntime& runtime, uint32_t graphicsQueueFamilyIndex, uint32_t computeQueueFamilyIndex)
{
    const VkDevice device = runtime.getDevice();

    presentCmdPool.init(device, graphicsQueueFamilyIndex);
    graphicsCmdPool.init(device, graphicsQueueFamilyIndex);
    computeCmdPool.init(device, computeQueueFamilyIndex);
    workerGraphicsPools.init(device, graphicsQueueFamilyIndex);

    const VkSemaphoreTypeCreateInfo timelineInfo{
        .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue  = 0,
    };
    const VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineInfo,
    };
    NVVK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore));
    NVVK_DBG_NAME(semaphore);
}

void VulkanRuntime::FrameData::deinit(VkDevice device)
{
    presentCmdPool.cleanup();
    graphicsCmdPool.cleanup();
    computeCmdPool.cleanup();
    workerGraphicsPools.cleanup();

    if (semaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(device, semaphore, nullptr);
        semaphore = VK_NULL_HANDLE;
    }

    presentCmdBuffer = VK_NULL_HANDLE;
    timelineValue    = 0;
}

void VulkanRuntime::FrameData::reset()
{
    presentCmdPool.reset();
    graphicsCmdPool.reset();
    computeCmdPool.reset();
    workerGraphicsPools.reset();
}

VulkanRuntime::VulkanRuntime(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo)
{
    if (Play::vkDriver && Play::vkDriver != this)
    {
        LOGE("Only one VulkanRuntime instance can be registered globally\n");
        return;
    }

    Play::vkDriver      = this;
    _registeredAsGlobal = true;

    init(config, contextInfo);
}

VulkanRuntime::~VulkanRuntime()
{
    destroy();
}

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
        destroy();
        return false;
    }
    _physicalDeviceProperties2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(getPhysicalDevice(), &_physicalDeviceProperties2);
    nvvk::DebugUtil::getInstance().init(getDevice());

    if (!initRenderServices())
    {
        destroy();
        return false;
    }

    if (!_window.createSurface(_context.getInstance(), &_surface))
    {
        destroy();
        return false;
    }
    NVVK_DBG_NAME(_surface);

    if (!createTransientCommandPool())
    {
        destroy();
        return false;
    }

    if (!initSurfaceAndSwapchain())
    {
        destroy();
        return false;
    }

    _renderSession = std::make_unique<Play::RenderSession>(Play::RenderSession::Info{.renderMode = _config.renderMode});
    getEditorRegistry().clear();
    if (!_renderSession->init())
    {
        destroy();
        return false;
    }

    _initialized = true;

    // editor system entrance
    Play::editor::RuntimeEditor& editor = _guiHost.getEditor();
    editor.bindRuntime(*this, *_renderSession, _config.renderMode.c_str());
    _guiHost.start();
    return true;
}

void VulkanRuntime::run()
{
    if (!_initialized)
    {
        return;
    }

    LOGI("Running SDL3 bootstrap runtime\n");

    while (!_window.shouldClose())
    {
        _window.pollEvents();
        if (_window.getInputState().keyOPressed)
        {
            _guiHost.start();
        }

        if (!_window.isRenderable())
        {
            SDL_Delay(10);
            continue;
        }

        if (!prepareFrame())
        {
            continue;
        }

        tick();
        _renderSession->beginFrame();
        _renderSession->renderFrame();
        signalPresentSemaphore();
        presentFrame();
    }

    if (_context.getDevice())
    {
        vkDeviceWaitIdle(_context.getDevice());
    }
}

void VulkanRuntime::destroy()
{
    if (!_initialized && !_context.getInstance() && !_window.isCreated())
    {
        if (_registeredAsGlobal && Play::vkDriver == this)
        {
            Play::vkDriver      = nullptr;
            _registeredAsGlobal = false;
        }
        return;
    }

    if (_context.getDevice())
    {
        vkDeviceWaitIdle(_context.getDevice());
    }

    _guiHost.stop();
    getEditorRegistry().clear();
    _renderSession.reset();
    clearSwapchainTextures();
    deinitRenderServices();
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
    if (_registeredAsGlobal && Play::vkDriver == this)
    {
        Play::vkDriver      = nullptr;
        _registeredAsGlobal = false;
    }
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

    const auto existingPreSelect         = info.preSelectPhysicalDeviceCallback;
    info.preSelectPhysicalDeviceCallback = [existingPreSelect](VkInstance instance, VkPhysicalDevice physicalDevice)
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
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && SDL_Vulkan_GetPresentationSupport(instance, physicalDevice, i))
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

bool VulkanRuntime::initRenderServices()
{
    _descriptorSetCache   = new Play::DescriptorSetCache();
    _pipelineCacheManager = new Play::PipelineCacheManager();

    Play::PlayResourceManager::Instance().initialize();
    Play::ShaderManager::Instance().init();

    if (!_enableDynamicRendering)
    {
        _renderPassCache  = new Play::RenderPassCache();
        _frameBufferCache = new Play::FrameBufferCache();
    }

    prepareGlobalDescriptorSet();
    prepareFrameDescriptorSet();
    _lastTick = SDL_GetPerformanceCounter();
    return true;
}

void VulkanRuntime::deinitRenderServices()
{
    if (!_descriptorSetCache && !_pipelineCacheManager)
    {
        return;
    }

    std::vector<Play::RefCounted*> leakedObjects = _registeredObjects;
    _registeredObjects.clear();
    for (Play::RefCounted* obj : leakedObjects)
    {
        if (obj && obj->isAlive())
        {
            LOGW("VulkanRuntime: Force destroying leaked RefCounted object at %p (refCount=%u)\n", obj, obj->getRefCount());
            obj->forceDestroy();
        }
    }

    destroyDeferredTasks();

    delete _frameBufferCache;
    _frameBufferCache = nullptr;
    delete _renderPassCache;
    _renderPassCache = nullptr;
    delete _descriptorSetCache;
    _descriptorSetCache = nullptr;
    delete _pipelineCacheManager;
    _pipelineCacheManager = nullptr;

    Play::PlayResourceManager::Instance().deInit();
    Play::ShaderManager::Instance().deInit();
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
    refreshCurrentSwapchainTexture();

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
    _deferredDestroyQueues.resize(frameCount);
    _frameIndex = 0;

    const uint32_t graphicsQueueFamilyIndex = getGfxQueue().familyIndex;
    const uint32_t computeQueueFamilyIndex  = getComputeQueue().familyIndex;

    for (FrameData& frame : _frames)
    {
        frame.init(*this, graphicsQueueFamilyIndex, computeQueueFamilyIndex);
    }

    return true;
}

void VulkanRuntime::destroyFrameSubmission()
{
    if (!_frames.empty() && _context.getDevice())
    {
        for (FrameData& frame : _frames)
        {
            frame.deinit(_context.getDevice());
        }
        _frames.clear();
    }
    _deferredDestroyQueues.clear();
    _pendingFrameWaitSemaphores.clear();
}

void VulkanRuntime::destroyDeferredTasks()
{
    for (DeferredDestroyQueue& queue : _deferredDestroyQueues)
    {
        for (auto& task : queue.tasks)
        {
            task();
        }
        queue.tasks.clear();
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
        .pSemaphores    = &frame.semaphore,
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

    clearSwapchainTextures();
    if (_swapchain.reinitResources(_windowSize, _config.vSync) != VK_SUCCESS)
    {
        LOGE("Swapchain rebuild failed\n");
        return false;
    }

    if (_swapchain.getFramesInFlight() != _frames.size())
    {
        if (!createFrameSubmission(_swapchain.getFramesInFlight()))
        {
            return false;
        }
    }
    else
    {
        _frameIndex = 0;
    }

    refreshCurrentSwapchainTexture();
    if (_renderSession)
    {
        _renderSession->onResize(_windowSize);
    }
    return true;
}

void VulkanRuntime::clearSwapchainTextures()
{
    _swapchainTextures.clear();
}

Play::Texture* VulkanRuntime::getCurrentSwapchainTexture()
{
    return refreshCurrentSwapchainTexture();
}

Play::Texture* VulkanRuntime::refreshCurrentSwapchainTexture()
{
    const uint32_t imageCount = _swapchain.getImageCount();
    if (imageCount == 0)
    {
        return nullptr;
    }

    const VkImage image = _swapchain.getImage();
    if (image == VK_NULL_HANDLE)
    {
        return nullptr;
    }

    for (RefPtr<Play::Texture>& texture : _swapchainTextures)
    {
        if (texture && texture->image == image)
        {
            return texture.get();
        }
    }

    _swapchainTextures.emplace_back(new Play::Texture(
        "SwapchainBackbuffer", image, _swapchain.getImageView(), _swapchain.getImageFormat(), {_windowSize.width, _windowSize.height, 1},
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_LAYOUT_UNDEFINED));
    return _swapchainTextures.back().get();
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
    _frames[_frameIndex].reset();
    _pendingFrameWaitSemaphores.clear();
    tryCleanupDeferredTasks();

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

    const VkSemaphoreSubmitInfo acquireWaitInfo{
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = _swapchain.getAcquireSemaphore(),
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    addWaitSemaphore(acquireWaitInfo);

    refreshCurrentSwapchainTexture();
    return true;
}

VkCommandBuffer VulkanRuntime::beginCommandRecording()
{
    FrameData& frame       = _frames[_frameIndex];
    frame.presentCmdBuffer = frame.presentCmdPool.allocCommandBuffer();

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    NVVK_CHECK(vkBeginCommandBuffer(frame.presentCmdBuffer, &beginInfo));
    return frame.presentCmdBuffer;
}

void VulkanRuntime::recordBootstrapClear(VkCommandBuffer cmd) const
{
    const float                     pulse = static_cast<float>(_frameCounter % 240) / 239.0F;
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
    frame.timelineValue        = signalValue;

    const VkCommandBufferSubmitInfo cmdInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };

    std::vector<VkSemaphoreSubmitInfo> waitInfos = consumePendingFrameWaitSemaphores();

    const VkSemaphoreSubmitInfo signalInfos[2] = {
        {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = _swapchain.getPresentSemaphore(),
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        },
        {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = frame.semaphore,
            .value     = signalValue,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
    };

    const VkSubmitInfo2 submitInfo{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = static_cast<uint32_t>(waitInfos.size()),
        .pWaitSemaphoreInfos      = waitInfos.empty() ? nullptr : waitInfos.data(),
        .commandBufferInfoCount   = 1,
        .pCommandBufferInfos      = &cmdInfo,
        .signalSemaphoreInfoCount = 2,
        .pSignalSemaphoreInfos    = signalInfos,
    };

    NVVK_CHECK(vkQueueSubmit2(_context.getQueueInfo(0).queue, 1, &submitInfo, nullptr));
}

void VulkanRuntime::signalPresentSemaphore()
{
    std::vector<VkSemaphoreSubmitInfo> waitInfos = consumePendingFrameWaitSemaphores();

    const VkSemaphoreSubmitInfo signalInfo{
        .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = _swapchain.getPresentSemaphore(),
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };

    const VkSubmitInfo2 submitInfo{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = static_cast<uint32_t>(waitInfos.size()),
        .pWaitSemaphoreInfos      = waitInfos.empty() ? nullptr : waitInfos.data(),
        .commandBufferInfoCount   = 0,
        .pCommandBufferInfos      = nullptr,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos    = &signalInfo,
    };

    NVVK_CHECK(vkQueueSubmit2(_context.getQueueInfo(0).queue, 1, &submitInfo, nullptr));
}

VkCommandBuffer VulkanRuntime::createTempCmdBuffer()
{
    const VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = _transientCmdPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    NVVK_CHECK(vkAllocateCommandBuffers(getDevice(), &allocInfo, &cmd));

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    NVVK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));
    return cmd;
}

void VulkanRuntime::submitAndWaitTempCmdBuffer(VkCommandBuffer cmd)
{
    NVVK_CHECK(vkEndCommandBuffer(cmd));

    const VkCommandBufferSubmitInfo cmdInfo{
        .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd,
    };
    const VkSubmitInfo2 submitInfo{
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos    = &cmdInfo,
    };
    NVVK_CHECK(vkQueueSubmit2(getGfxQueue().queue, 1, &submitInfo, nullptr));
    NVVK_CHECK(vkQueueWaitIdle(getGfxQueue().queue));
    vkFreeCommandBuffers(getDevice(), _transientCmdPool, 1, &cmd);
}

void VulkanRuntime::addWaitSemaphore(const VkSemaphoreSubmitInfo& signalInfo)
{
    _pendingFrameWaitSemaphores.push_back(signalInfo);
}

std::vector<VkSemaphoreSubmitInfo> VulkanRuntime::consumePendingFrameWaitSemaphores()
{
    std::vector<VkSemaphoreSubmitInfo> waitInfos = _pendingFrameWaitSemaphores;
    _pendingFrameWaitSemaphores.clear();
    return waitInfos;
}

void VulkanRuntime::deferDestroy(std::function<void()> task)
{
    if (_deferredDestroyQueues.empty())
    {
        task();
        return;
    }

    const uint32_t targetFrame = (_frameIndex + 1) % static_cast<uint32_t>(_deferredDestroyQueues.size());
    _deferredDestroyQueues[targetFrame].tasks.push_back(std::move(task));
}

void VulkanRuntime::registerObject(Play::RefCounted* obj)
{
    if (!obj)
    {
        return;
    }

    for (Play::RefCounted* registeredObject : _registeredObjects)
    {
        if (registeredObject == obj)
        {
            return;
        }
    }
    _registeredObjects.push_back(obj);
}

void VulkanRuntime::unregisterObject(Play::RefCounted* obj)
{
    for (size_t i = 0; i < _registeredObjects.size(); ++i)
    {
        if (_registeredObjects[i] == obj)
        {
            _registeredObjects[i] = _registeredObjects.back();
            _registeredObjects.pop_back();
            return;
        }
    }
}

void VulkanRuntime::tryCleanupDeferredTasks()
{
    if (_deferredDestroyQueues.empty())
    {
        return;
    }

    DeferredDestroyQueue& queue = _deferredDestroyQueues[_frameIndex];
    for (auto& task : queue.tasks)
    {
        task();
    }
    queue.tasks.clear();
}

void VulkanRuntime::tick()
{
    const uint64_t currentTick = SDL_GetPerformanceCounter();
    const uint64_t frequency   = SDL_GetPerformanceFrequency();
    _deltaTime                 = _lastTick == 0 ? 0.0 : static_cast<double>(currentTick - _lastTick) / static_cast<double>(frequency);
    _lastTick                  = currentTick;
}

void VulkanRuntime::prepareGlobalDescriptorSet()
{
    _globalDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr);
    _globalDescriptorBindings.addBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr);
    _globalDescriptorBindings.addBinding(2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr);
    _globalDescriptorBindings.addBinding(3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr);
    _globalDescriptorBindings.addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr);
    _descriptorSetCache->initGlobalDescriptorSets(_globalDescriptorBindings);
    updateGlobalDescriptorSet();
}

void VulkanRuntime::updateGlobalDescriptorSet()
{
    std::vector<VkSampler>             samplerList(2);
    std::vector<VkDescriptorImageInfo> imageInfoList;
    VkSamplerCreateInfo                samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.magFilter     = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter     = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipmapMode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias    = 0.0F;
    samplerCreateInfo.maxLod        = VK_LOD_CLAMP_NONE;
    samplerCreateInfo.maxAnisotropy = 1.0F;
    samplerCreateInfo.compareEnable = VK_FALSE;
    Play::PlayResourceManager::Instance().acquireSampler(samplerList[0], samplerCreateInfo);
    imageInfoList.push_back({samplerList[0]});
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    Play::PlayResourceManager::Instance().acquireSampler(samplerList[1], samplerCreateInfo);
    imageInfoList.push_back({samplerList[1]});

    std::vector<VkWriteDescriptorSet> writeSet(2, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET});
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

    vkUpdateDescriptorSets(getDevice(), static_cast<uint32_t>(writeSet.size()), writeSet.data(), 0, nullptr);
}

void VulkanRuntime::updateGlobalTonemapperBuffer(Play::Buffer* buffer)
{
    if (!buffer || !_descriptorSetCache)
    {
        return;
    }

    VkDescriptorBufferInfo toneMappingBufferInfo{};
    toneMappingBufferInfo.buffer = buffer->buffer;
    toneMappingBufferInfo.offset = 0;
    toneMappingBufferInfo.range  = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writeSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeSet.dstSet          = _descriptorSetCache->getEngineDescriptorSet().set;
    writeSet.dstBinding      = 4;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeSet.pBufferInfo     = &toneMappingBufferInfo;

    vkUpdateDescriptorSets(getDevice(), 1, &writeSet, 0, nullptr);
}

void VulkanRuntime::prepareFrameDescriptorSet()
{
    _frameDescriptorBindings.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr);
    _descriptorSetCache->initFrameDescriptorSets(_frameDescriptorBindings);
}

void VulkanRuntime::updateFrameDescriptorSet() {}

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
