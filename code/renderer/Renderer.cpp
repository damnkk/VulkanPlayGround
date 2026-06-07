#include "Renderer.h"
#include "Resource.h"
#include "core/PlayCamera.h"
#include "SceneManager.h"
#include "VulkanDriver.h"
#include "RDG/RDG.h"
#include "renderPasses/RenderPass.h"
#include "renderPasses/PresentPass.h"
#include <algorithm>
namespace Play
{

namespace
{
GpuSceneType getRuntimeGpuSceneType()
{
    if (!vkDriver)
    {
        return GpuSceneType::eRaster;
    }

    switch (vkDriver->getRenderMode())
    {
        case RenderSession::eGaussianRendering:
            return GpuSceneType::eGaussian;
        case RenderSession::eRayTracing:
            return GpuSceneType::eRayTracing;
        default:
            return GpuSceneType::eRaster;
    }
}
} // namespace

void Renderer::addCamera()
{
    auto camera = std::make_unique<PlayCamera>();
    camera->getCameraManipulator()->setMode(nvutils::CameraManipulator::Modes::Fly);
    _cameras.push_back(std::move(camera));
}

void Renderer::setActiveCamera(size_t index)
{
    _activeCameraIdx = index;
}

Buffer* Renderer::getCurrentCameraBuffer() const
{
    return _cameraUniformData[vkDriver->getFrameCycleIndex()].get();
}

const CameraData& Renderer::getCurrentCameraData() const
{
    return _cameraDatas[vkDriver->getFrameCycleIndex()];
}

Renderer::Renderer()
{
    addCamera();

    _scene = std::make_unique<SceneManager>(getRuntimeGpuSceneType());

    for (int i = 0; i < _cameraUniformData.size(); ++i)
    {
        _cameraUniformData[i] = RefPtr<Buffer>(new Buffer("cameraInfoBuf" + std::to_string(i), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT, sizeof(CameraData),
                                               VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    }
}

Renderer::~Renderer() = default;

void Renderer::updateCameraBuffer()
{
    // update curr activated camera into camera buffer
    PlayCamera* camera = getActiveCamera();
    camera->update(vkDriver->getInputState(), static_cast<float>(vkDriver->getDeltaTime()));
    CameraData data{};
    data.cameraPosition    = camera->getCameraManipulator()->getEye();
    data.projMatrix        = camera->getCameraManipulator()->getPerspectiveMatrix();
    data.viewMatrix        = camera->getCameraManipulator()->getViewMatrix();
    data.viewPortSize      = {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height};
    data.viewProjMatrix    = data.projMatrix * data.viewMatrix;
    data.invViewMatrix     = glm::inverse(data.viewMatrix);
    data.invProjMatrix     = glm::inverse(data.projMatrix);
    data.invViewProjMatrix = glm::inverse(data.viewProjMatrix);

    _cameraDatas[vkDriver->getFrameCycleIndex()] = data;
    memcpy(getCurrentCameraBuffer()->mapping, &data, sizeof(CameraData));
    PlayResourceManager::Instance().flushBuffer(*getCurrentCameraBuffer(), 0, VK_WHOLE_SIZE);
}

void Renderer::OnPreRender()
{
    updatePresentTexture();
    updateCameraBuffer();
    _scene->update();
}

void Renderer::RenderFrame()
{
    _rdgBuilder->execute();
}

void Renderer::OnPostRender() {}

void Renderer::SetScene(SceneManager* scene) {}

void Renderer::OnResize(int width, int height)
{
    width  = std::max(width, 1);
    height = std::max(height, 1);

    for (auto& camera : _cameras)
    {
        camera->onResize({(uint32_t)width, (uint32_t)height});
    }

    _rdgBuilder.reset();
    _rdgBuilder = std::make_unique<RDG::RDGBuilder>();

    if (_passes.empty())
    {
        setupPasses();
        for (auto& pass : _passes)
        {
            pass->init();
        }
    }

    for (auto& pass : _passes)
    {
        pass->build(_rdgBuilder.get());
    }
    _rdgBuilder->compile();
}

void Renderer::updatePresentTexture()
{
    if (!_rdgBuilder || !vkDriver)
    {
        return;
    }

    RDG::RDGTextureRef presentTexture = _rdgBuilder->getTexture(PresentPass::PRESENT_TEXTURE_NAME);
    if (!presentTexture)
    {
        return;
    }

    Texture* swapchainTexture = vkDriver->getCurrentSwapchainTexture();
    if (!swapchainTexture)
    {
        return;
    }

    presentTexture->setRHI(swapchainTexture, false);
}

} // namespace Play
