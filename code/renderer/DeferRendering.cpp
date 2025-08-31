#include "DeferRendering.h"
namespace Play
{

DeferRenderer:: DeferRenderer(PlayElement& app)
{
    // add pass
    //graph compile(pass clip / parepare resource)
}
DeferRenderer::~DeferRenderer() {}
void            DeferRenderer::OnPreRender() {}
void            DeferRenderer::OnPostRender() {}
void            DeferRenderer::RenderFrame()
{
    // invoke lambda function in RDGPass
}
void            DeferRenderer::SetScene(Scene* scene) {}
void            DeferRenderer::OnResize(int width, int height) {}
void            DeferRenderer::OnDestroy() {}
} // namespace Play