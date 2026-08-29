#include "GaussianRenderer.h"
#include "GaussianPass/GaussianSortPass.h"
#include "GaussianPass/GaussianDrawMeshPass.h"
#include "core/runtime/VulkanRuntime.h"
#include "resourceManagement/scene/SceneManager.h"
namespace Play
{

GaussianRenderer::GaussianRenderer(RenderSession& session)
{
    _view = &session;
    // vkDriver->getSceneManager()->getGaussianScene().load("D:/repo/ml-sharp/output/test.ply");
    // vkDriver->getSceneManager()->getGaussianScene().load("D:/repo/ml-sharp/output/DSC_0065.ply");
    vkDriver->getSceneManager()->getGaussianScene().load("D:/repo/vk_gaussian_splatting/_downloaded_resources/flowers_1/flowers_1.ply");
}

GaussianRenderer::~GaussianRenderer() {}

void GaussianRenderer::setupPasses()
{
    _passes.emplace_back(std::make_unique<GaussianSortPass>(this));
    _passes.emplace_back(std::make_unique<GaussianDrawMeshPass>(this));
}

} // namespace Play
