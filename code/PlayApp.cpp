#include "PlayApp.h"
#include "nvvk/debug_util.hpp"
#define IMGUI_DEFINE_MATH_OPERATORS
#include <backends/imgui_impl_vulkan.h>
#include "stb_image.h"
#include "DeferRendering.h"
#include "GaussianRenderer.h"
#include "resourceManagement/Resource.h"
#include "ShaderManager.hpp"
#include "PlayAllocator.h"
#include "PipelineCacheManager.h"
#include "RenderPassCache.h"
#include "FrameBufferCache.h"
#include "VulkanDriver.h"
#include "controlComponent/controlComponent.h"
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
    }
};
RenderSession::RenderSession(Info info) : _info(info)
{
    // 解析命令行渲染模式
    if (_info.renderMode && !_info.renderMode->empty())
    {
        const std::string& mode = *_info.renderMode;
        if (mode == "raster")
            _renderMode = eRasterization;
        else if (mode == "raytrace")
            _renderMode = eRayTracing;
        else if (mode == "volume")
            _renderMode = eVolumeRendering;
        else if (mode == "shadingrate")
            _renderMode = eShadingRateRendering;
        else if (mode == "defer")
            _renderMode = eDeferRendering;
        else if (mode == "gaussian")
            _renderMode = eGaussianRendering;
    }
}

RenderSession::~RenderSession()
{
    // delete _sceneManager;
    // _sceneManager = nullptr;
}

void RenderSession::onAttach(nvapp::Application* app)
{
    _app = app;
    // CameraManip
    // _sceneManager     = new SceneManager();
    _profilerTimeline = _info.profilerManager->createTimeline({"graphics"});
    _profilerGpuTimer.init(_profilerTimeline, app->getDevice(), app->getPhysicalDevice(), app->getQueue(0).familyIndex, true);
    switch (_renderMode)
    {
        case eDeferRendering:
        {
            _renderer = std::make_unique<DeferRenderer>(*this);
            break;
        }
        case eGaussianRendering:
        {
            _renderer = std::make_unique<GaussianRenderer>(*this);
            break;
        }
        default:
        {
            LOGE("Unsupported render mode, defaulting to DeferRendering");
        }
    }
}

void RenderSession::onDetach()
{
    vkQueueWaitIdle(_app->getQueue(0).queue);

    _profilerGpuTimer.deinit();
    _info.profilerManager->destroyTimeline(_profilerTimeline);
    _renderer.reset();
}

void RenderSession::onResize(VkCommandBuffer cmd, const VkExtent2D& size)
{
    vkQueueWaitIdle(_app->getQueue(0).queue);
    _renderer->OnResize(size.width, size.height);
}

void RenderSession::onUIRender()
{
    ImGui::Begin("Viewport");
    ImGui::End();
    vkDriver->getTonemapperControlComponent().onGUI();
    _renderer->OnGUI();
}

void RenderSession::onUIMenu() {}

void RenderSession::onPreRender()
{
    vkDriver->tick();
    _renderer->OnPreRender();
    vkDriver->tryCleanupDeferredTasks();
}

void RenderSession::onRender(VkCommandBuffer cmd)
{
    if (_app->getViewportSize().width == 0 || _app->getViewportSize().height == 0)
    {
        return;
    }

    vkDriver->getCurrentFrameData().reset();

    _renderer->RenderFrame();
    _renderer->OnPostRender();
}

void RenderSession::onFileDrop(const std::filesystem::path& filename)
{
    // Handle file drop events here
    LOGI("File dropped: %s", filename.string().c_str());
}

void RenderSession::onLastHeadlessFrame()
{
    // Handle last frame in headless mode
    LOGI("Last headless frame");
}

} // namespace Play
