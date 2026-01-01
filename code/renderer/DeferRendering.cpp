#include "DeferRendering.h"
#include "PlayApp.h"
#include "renderpasses/PostProcessPass.h"
#include "renderPasses/PresentPass.h"
#include "core/PlayCamera.h"
#include "VulkanDriver.h"
namespace Play
{

DeferRenderer::DeferRenderer(PlayElement& view)
{
    _view          = &view;
    _cameraInfoBuf = Buffer::Create("cameraInfoBuf", VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT, sizeof(CameraData),
                                    VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}
DeferRenderer::~DeferRenderer() {}
void DeferRenderer::OnPreRender()
{
    updateCameraBuffer();
    updateSceneAnimationBuffer();
}
void DeferRenderer::OnPostRender() {}
void DeferRenderer::RenderFrame()
{
    _rdgBuilder->execute();
}
void DeferRenderer::SetScene(SceneManager* scene) {}
void DeferRenderer::OnResize(int width, int height)
{
    _passes.clear();
    _rdgBuilder.reset();
    _rdgBuilder    = std::make_unique<RDG::RDGBuilder>();
    _outputTexture = _view->getUITexture();
    // add logic pass
    _passes.push_back(std::make_unique<PostProcessPass>(_view));
    _passes.push_back(std::make_unique<PresentPass>(_view));
    for (auto& pass : _passes)
    {
        pass->init();
    }
    for (auto& pass : _passes)
    {
        pass->build(_rdgBuilder.get());
    }
    _rdgBuilder->compile();
}

void DeferRenderer::updateCameraBuffer()
{
    // update curr activated camera into camera buffer
    PlayCamera* camera = getActiveCamera();
    camera->update(ImGui::FindWindowByName("Viewport"));
    CameraData data{};
    data.cameraPosition    = camera->getCameraManipulator()->getEye();
    data.projMatrix        = camera->getCameraManipulator()->getPerspectiveMatrix();
    data.viewMatrix        = camera->getCameraManipulator()->getViewMatrix();
    data.viewPortSize      = {vkDriver->getApp()->getViewportSize().width, vkDriver->getApp()->getViewportSize().height};
    data.viewProjMatrix    = data.projMatrix * data.viewMatrix;
    data.invViewMatrix     = glm::inverse(data.viewMatrix);
    data.invProjMatrix     = glm::inverse(data.projMatrix);
    data.invViewProjMatrix = glm::inverse(data.viewProjMatrix);

    memcpy(_cameraInfoBuf->mapping, &data, sizeof(CameraData));
    PlayResourceManager::Instance().flushBuffer(*_cameraInfoBuf, 0, VK_WHOLE_SIZE);
}

void DeferRenderer::updateSceneAnimationBuffer()
{
    _scene->updateAnimationBuffer();
}
} // namespace Play