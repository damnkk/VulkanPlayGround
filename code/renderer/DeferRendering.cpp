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
    _scene->addScene<nvvkgltf::Scene>("D:/repo/downloaded_resources/man/SK_Man_Full_04.gltf");
    // _scene->addScene("D:/repo/VulkanPlayGround/resource/models/rimac-nevera-r-2025-wwwvecarzcom/scene.gltf");
    std::filesystem::path modelPath = ".\\resource\\skybox\\graveyard_pathways_2k.hdr";
    // std::filesystem::path modelPath     = ".\\resource\\skybox\\graveyard_pathways_2k.hdr";
    Texture* skyboxTexture = Texture::Create(modelPath, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, true);
    _scene->addSkyBoxTexture(skyboxTexture);
    _scene->updateDescriptorSet();
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

} // namespace Play