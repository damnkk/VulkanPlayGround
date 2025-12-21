#include "DeferRendering.h"
#include "PlayApp.h"
#include "renderpasses/PostProcessPass.h"
#include "renderPasses/PresentPass.h"

namespace Play
{

DeferRenderer::DeferRenderer(PlayElement& view)
{
    _view = &view;
}
DeferRenderer::~DeferRenderer() {}
void DeferRenderer::OnPreRender() {}
void DeferRenderer::OnPostRender() {}
void DeferRenderer::RenderFrame()
{
    _rdgBuilder->execute();
}
void DeferRenderer::SetScene(Scene* scene) {}
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
} // namespace Play