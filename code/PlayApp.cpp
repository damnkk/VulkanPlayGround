#include "PlayApp.h"
#include "nvvk/debug_util.hpp"
#include "nvvk/check_error.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_vulkan.h>
#include "stb_image.h"
#include "DeferRendering.h"
#include "resourceManagement/Resource.h"
#include "ShaderManager.hpp"
#include "PlayAllocator.h"
#include "PipelineCacheManager.h"
#include "RenderPassCache.h"
#include "FrameBufferCache.h"
namespace Play
{
struct ScopeTimer
{
    std::chrono::high_resolution_clock::time_point start;
    ScopeTimer() : start(std::chrono::high_resolution_clock::now()) {}
    ~ScopeTimer()
    {
        auto end     = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Scene traversal time: " << elapsed.count() << " ms" << std::endl;
    }
};
PlayElement::PlayElement(Info info) : _info(info) {}

PlayElement::~PlayElement()
{
    _descriptorSetCache.reset();
    for (auto& frame : _frameData)
    {
        vkDestroySemaphore(_app->getDevice(), frame.semaphore, nullptr);
        vkDestroyCommandPool(_app->getDevice(), frame.graphicsCmdPool, nullptr);
        vkDestroyCommandPool(_app->getDevice(), frame.computeCmdPool, nullptr);
    }
}

void PlayElement::onAttach(nvapp::Application* app)
{
    _app = app;
    nvvk::DebugUtil::getInstance().init(_app->getDevice());
    // _modelLoader.init(this);
    // CameraManip
    PlayResourceManager::Instance().initialize(this);
    TexturePool::Instance().init(65535, &PlayResourceManager::Instance());
    BufferPool::Instance().init(65535, &PlayResourceManager::Instance());
    ShaderManager::Instance().init(this);
    PipelineCacheManager::Instance().init(this);
    _descriptorSetCache = std::make_unique<DescriptorSetCache>(this);
    _frameData.resize(_app->getFrameCycleSize());
    if (!_enableDynamicRendering)
    {
        _renderPassCache  = std::make_unique<RenderPassCache>(this);
        _frameBufferCache = std::make_unique<FrameBufferCache>(this);
    }
    for (size_t i = 0; i < _frameData.size(); ++i)
    {
        VkCommandPoolCreateInfo cmdPoolCI{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolCI.queueFamilyIndex = _app->getQueue(0).familyIndex;
        NVVK_CHECK(vkCreateCommandPool(_app->getDevice(), &cmdPoolCI, nullptr, &_frameData[i].graphicsCmdPool));
        cmdPoolCI.queueFamilyIndex = _app->getQueue(1).familyIndex;
        NVVK_CHECK(vkCreateCommandPool(_app->getDevice(), &cmdPoolCI, nullptr, &_frameData[i].computeCmdPool));
        VkSemaphoreTypeCreateInfo timelineCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue  = 0;

        VkSemaphoreCreateInfo semaphoreCreateInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        semaphoreCreateInfo.flags = 0;
        semaphoreCreateInfo.pNext = &timelineCreateInfo;

        NVVK_CHECK(vkCreateSemaphore(_app->getDevice(), &semaphoreCreateInfo, nullptr, &_frameData[i].semaphore));
    }

    _profilerTimeline = _info.profilerManager->createTimeline({"graphics"});
    _profilerGpuTimer.init(_profilerTimeline, app->getDevice(), app->getPhysicalDevice(), app->getQueue(0).familyIndex, true);
    createGraphicsDescriptResource();

    _sceneManager.init(this);

    switch (_renderMode)
    {
        case eDeferRendering:
        {
            _renderer = std::make_unique<DeferRenderer>(*this);
            break;
        }
        default:
        {
            LOGE("Unsupported render mode, defaulting to DeferRendering");
        }
    }
    nvvk::DebugUtil::getInstance().setObjectName(_uiTexture->image, "PlayElement_UITexture");
}

void PlayElement::onDetach()
{
    vkQueueWaitIdle(_app->getQueue(0).queue);
    ImGui_ImplVulkan_RemoveTexture(_uiTextureDescriptor);
    TexturePool::Instance().deinit();
    BufferPool::Instance().deinit();
    PlayResourceManager::Instance().deInit();
    ShaderManager::Instance().deInit();
    _profilerGpuTimer.deinit();
    _info.profilerManager->destroyTimeline(_profilerTimeline);
    PipelineCacheManager::Instance().deinit();
}

void PlayElement::onResize(VkCommandBuffer cmd, const VkExtent2D& size)
{
    vkQueueWaitIdle(_app->getQueue(0).queue);
    _renderer->OnResize(size.width, size.height);
    auto task = [uiTextureDescriptor = _uiTextureDescriptor, uiTexture = _uiTexture]()
    {
        ImGui_ImplVulkan_RemoveTexture(uiTextureDescriptor);
        Texture::Destroy(uiTexture);
    };
    _deferredDeleteTaskQueue.push({getFrameCycleIndex(), task});

    createGraphicsDescriptResource();
}

void PlayElement::onUIRender()
{
    ImGui::Begin("Viewport");
    ImGui::Image((ImTextureID) _uiTextureDescriptor, ImGui::GetContentRegionAvail());
    ImGui::End();
}

void PlayElement::onUIMenu() {}

void PlayElement::onPreRender()
{
    _renderer->OnPreRender();
    while (!_deferredDeleteTaskQueue.empty())
    {
        auto& taskPair = _deferredDeleteTaskQueue.front();
        if ((uint32_t) taskPair.first != _app->getFrameCycleIndex()) break;
        taskPair.second();
        _deferredDeleteTaskQueue.pop();
    }
}

void PlayElement::onRender(VkCommandBuffer cmd)
{
    // VkClearColorValue       clearColor = {{0.91f, 0.23f, 0.77f, 1.0f}};
    // VkImageSubresourceRange range      = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    // nvvk::cmdImageMemoryBarrier(cmd, {_uiTexture->image,
    // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL});
    // vkCmdClearColorImage(cmd, _uiTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    // &clearColor,
    //                      1, &range);
    // nvvk::cmdImageMemoryBarrier(cmd, {_uiTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    //                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    _frameNum++;
    PlayFrameData& frameData = _frameData[_app->getFrameCycleIndex()];
    frameData.reset(getDevice());

    _renderer->RenderFrame();
    _renderer->OnPostRender();
    nvvk::BarrierContainer barrierContainer;
    VkImageMemoryBarrier2  uiImageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    uiImageBarrier.image            = _uiTexture->image;
    uiImageBarrier.oldLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    uiImageBarrier.newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    uiImageBarrier.srcAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    uiImageBarrier.dstAccessMask    = VK_ACCESS_2_SHADER_READ_BIT;
    uiImageBarrier.srcStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    uiImageBarrier.dstStageMask     = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    uiImageBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrierContainer.appendOptionalLayoutTransition(*_uiTexture, uiImageBarrier);
    barrierContainer.cmdPipelineBarrier(cmd, 0);
}

void PlayElement::onFileDrop(const std::filesystem::path& filename)
{
    // Handle file drop events here
    LOGI("File dropped: %s", filename.string().c_str());
}

void PlayElement::onLastHeadlessFrame()
{
    // Handle last frame in headless mode
    LOGI("Last headless frame");
}

void PlayElement::createGraphicsDescriptResource()
{
    _uiTexture = Texture::Create(_app->getWindowSize().width, _app->getWindowSize().height, VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);
    PlayResourceManager::Instance().acquireSampler(_uiTexture->descriptor.sampler);
    _uiTextureDescriptor =
        ImGui_ImplVulkan_AddTexture(_uiTexture->descriptor.sampler, _uiTexture->descriptor.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

std::filesystem::path getBaseFilePath()
{
    return "./";
}

} // namespace Play