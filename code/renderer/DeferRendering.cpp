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
    std::filesystem::path modelPath = ".\\resource\\skybox\\graveyard_pathways_2k.hdr";
    Texture* skyboxTexture = Texture::Create(modelPath, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, true);
    _scene->addSkyBoxTexture(skyboxTexture);
    _scene->updateDescriptorSet();
}

DeferRenderer::~DeferRenderer()
{
    int a = 0;
}

void DeferRenderer::OnGUI()
{
    ImGui::Begin("Camera Profile");
    nvgui::CameraWidget(getActiveCamera()->getCameraManipulator());
    ImGui::End();
}

void DeferRenderer::setupPasses()
{
    _passes.push_back(std::make_unique<VolumeSkyPass>(this));
    _passes.push_back(std::make_unique<GBufferPass>(this));
    _passes.push_back(std::make_unique<LightPass>(this));
    _passes.push_back(std::make_unique<PostProcessPass>(this));
    _passes.push_back(std::make_unique<PresentPass>(_view));
}

} // namespace Play