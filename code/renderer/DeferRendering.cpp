#include "DeferRendering.h"
#include "PlayApp.h"
#include "renderpasses/PostProcessPass.h"

namespace Play
{

DeferRenderer::DeferRenderer(PlayElement& view) : _rdgBuilder(&view)
{
    _outputTexture = view.getUITexture();
    // add logic pass
    _passes.push_back(std::make_unique<PostProcessPass>(&view));
    for (auto& pass : _passes)
    {
        pass->init();
    }
    for (auto& pass : _passes)
    {
        pass->build(&_rdgBuilder);
    }
    _rdgBuilder.compile();
}
DeferRenderer::~DeferRenderer() {}
void DeferRenderer::OnPreRender() {}
void DeferRenderer::OnPostRender() {}
void DeferRenderer::RenderFrame()
{
    _rdgBuilder.execute();
}
void DeferRenderer::SetScene(Scene* scene) {}
void DeferRenderer::OnResize(int width, int height) {}
} // namespace Play