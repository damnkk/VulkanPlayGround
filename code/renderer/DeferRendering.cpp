#include "DeferRendering.h"
#include "core/runtime/RenderSession.h"
#include "renderpasses/PostProcessPass.h"
#include "renderPasses/PresentPass.h"
#include "renderPasses/VolumeSkyPass.h"
#include "renderPasses/GBufferPass.h"
#include "renderPasses/LightPass.h"
#include "SceneManager.h"
#include "core/runtime/VulkanRuntime.h"
namespace Play
{

DeferRenderer::DeferRenderer(RenderSession& session) 
{
    _view = &session;
    // TODO: Load raster models through CpuModelComponent once the component-owned loader is wired.
    std::filesystem::path modelPath     = ".\\resource\\skybox\\graveyard_pathways_2k.hdr";
    RefPtr<Texture>       skyboxTexture = RefPtr<Texture>(new Texture(modelPath, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, true));
    _scene->addSkyBoxTexture(skyboxTexture);
    _scene->updateDescriptorSet();
}

DeferRenderer::~DeferRenderer()
{
    int a = 0;
}

void DeferRenderer::setupPasses()
{
    _passes.push_back(std::make_unique<VolumeSkyPass>(this));
    _passes.push_back(std::make_unique<GBufferPass>(this));
    _passes.push_back(std::make_unique<LightPass>(this));
    _passes.push_back(std::make_unique<PostProcessPass>(this));
    _passes.push_back(std::make_unique<PresentPass>(this));
}

} // namespace Play
