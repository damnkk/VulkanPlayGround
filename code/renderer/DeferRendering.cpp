#include <nvgui/camera.hpp>
#include "DeferRendering.h"
#include "PlayApp.h"
#include "renderpasses/PostProcessPass.h"
#include "renderPasses/PresentPass.h"
#include "renderPasses/VolumeSkyPass.h"
#include "renderPasses/GBufferPass.h"
#include "renderPasses/LightPass.h"
#include "core/PlayCamera.h"
#include "VulkanDriver.h"
namespace Play
{

DeferRenderer::DeferRenderer(PlayElement& view)
{
    _view = &view;
}
DeferRenderer::~DeferRenderer()
{
    int a = 0;
}
void DeferRenderer::OnPreRender()
{
    updateCameraBuffer();
    _scene->update();
}

void DeferRenderer::OnGUI()
{
    ImGui::Begin("Camera Profile");
    nvgui::CameraWidget(getActiveCamera()->getCameraManipulator());
    ImGui::End();
}
void DeferRenderer::OnPostRender() {}
void DeferRenderer::RenderFrame()
{
    _rdgBuilder->execute();
}
void DeferRenderer::SetScene(SceneManager* scene) {}
void DeferRenderer::OnResize(int width, int height)
{
    for (auto& camera : _cameras)
    {
        camera->onResize({(uint32_t) width, (uint32_t) height});
    }
    _passes.clear();
    _rdgBuilder.reset();
    _rdgBuilder    = std::make_unique<RDG::RDGBuilder>();
    _outputTexture = _view->getUITexture();
    // add logic pass
    _passes.push_back(std::make_unique<VolumeSkyPass>(this));
    _passes.push_back(std::make_unique<GBufferPass>(this));
    _passes.push_back(std::make_unique<LightPass>(this));
    _passes.push_back(std::make_unique<PostProcessPass>(this));
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

    memcpy(getCurrentCameraBuffer()->mapping, &data, sizeof(CameraData));
    PlayResourceManager::Instance().flushBuffer(*getCurrentCameraBuffer(), 0, VK_WHOLE_SIZE);
}

} // namespace Play