#include "VolumeRenderer.h"

#include "renderPasses/PresentPass.h"
#include "renderPasses/VolumeRenderPass.h"

namespace Play
{

VolumeRenderer::VolumeRenderer(RenderSession& session)
{
    _view = &session;
}

VolumeRenderer::~VolumeRenderer() = default;

void VolumeRenderer::OnPreRender()
{
    Renderer::OnPreRender();
    if (_volumePass)
    {
        _volumePass->updateUniform();
    }
}

void VolumeRenderer::OnResize(int width, int height)
{
    if (_volumePass)
    {
        _volumePass->resetFrameCount();
    }
    Renderer::OnResize(width, height);
}

void VolumeRenderer::setupPasses()
{
    auto volumePass = std::make_unique<VolumeRenderPass>(this);
    _volumePass = volumePass.get();
    _passes.emplace_back(std::move(volumePass));
    _passes.emplace_back(std::make_unique<PresentPass>(this));
}

} // namespace Play
