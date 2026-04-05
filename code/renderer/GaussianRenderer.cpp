#include "GaussianRenderer.h"
#include "GaussianPass/GaussianSortPass.h"
#include "GaussianPass/GaussianDrawMeshPass.h"
namespace Play
{

GaussianRenderer::GaussianRenderer(PlayElement& element)
{
    _outputTexture = element.getUITexture();
    _view          = &element;
    // _scene->addScene<GaussianScene>("D:/repo/ml-sharp/output/test.ply");
    // _scene->addScene<GaussianScene>("D:/repo/ml-sharp/output/DSC_0065.ply");
    _scene->addScene<GaussianScene>("D:/repo/vk_gaussian_splatting/_downloaded_resources/flowers_1/flowers_1.ply");
}

GaussianRenderer::~GaussianRenderer() {}

void GaussianRenderer::OnGUI() {}

void GaussianRenderer::setupPasses()
{
    _passes.emplace_back(std::make_unique<GaussianSortPass>(this));
    _passes.emplace_back(std::make_unique<GaussianDrawMeshPass>(this));
}

} // namespace Play