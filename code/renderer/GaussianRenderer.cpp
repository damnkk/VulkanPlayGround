#include "GaussianRenderer.h"
#include "renderPasses/RenderPass.h"
#include "core/PlayCamera.h"
namespace Play
{

GaussianRenderer::GaussianRenderer(PlayElement& element)
{
    _outputTexture = element.getUITexture();
    _view          = &element;
}
GaussianRenderer::~GaussianRenderer() {}

void GaussianRenderer::OnGUI() {}
void GaussianRenderer::OnPreRender()
{
    updateCameraBuffer();
}

void GaussianRenderer::RenderFrame() {}

void GaussianRenderer::OnPostRender() {}
void GaussianRenderer::SetScene(SceneManager* scene) {}

void GaussianRenderer::OnResize(int width, int height) {}

} // namespace Play