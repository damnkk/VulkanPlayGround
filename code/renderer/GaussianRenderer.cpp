#include "GaussianRenderer.h"
#include "GaussianPass/GaussianSortPass.h"
namespace Play
{

GaussianRenderer::GaussianRenderer(PlayElement& element)
{
    _outputTexture = element.getUITexture();
    _view          = &element;
    _scene->addScene<GaussianScene>("D:/repo/ml-sharp/output/test.ply");
}

GaussianRenderer::~GaussianRenderer() {}

void GaussianRenderer::OnGUI() {}

void GaussianRenderer::setupPasses()
{
    _passes.emplace_back(std::make_unique<GaussianSortPass>(this));
}

} // namespace Play