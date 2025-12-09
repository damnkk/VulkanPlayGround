#include "PlayApp.h"
#include "nvvk/debug_util.hpp"
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
#include "VulkanDriver.h"
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
    delete vkDriver;
    vkDriver = nullptr;
}

void PlayElement::onAttach(nvapp::Application* app)
{
    _app     = app;
    vkDriver = new VulkanDriver(app);
    vkDriver->init();
    // CameraManip

    _profilerTimeline = _info.profilerManager->createTimeline({"graphics"});
    _profilerGpuTimer.init(_profilerTimeline, app->getDevice(), app->getPhysicalDevice(), app->getQueue(0).familyIndex, true);
    createGraphicsDescriptResource();

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

    _profilerGpuTimer.deinit();
    _info.profilerManager->destroyTimeline(_profilerTimeline);
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
    vkDriver->_deferredDeleteTaskQueue.push({vkDriver->getFrameCycleIndex(), task});

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
    vkDriver->tick();
    _renderer->OnPreRender();
    vkDriver->tryCleanupDeferredTasks();
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

    vkDriver->getCurrentFrameData().reset(vkDriver->_device);

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

} // namespace Play