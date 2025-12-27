#include "Renderer.h"
#include "core/PlayCamera.h"
namespace Play
{

void Renderer::addCamera()
{
    auto camera = std::make_unique<PlayCamera>();
    _cameras.push_back(std::move(camera));
}

void Renderer::setActiveCamera(size_t index)
{
    _activeCameraIdx = index;
}

Renderer::Renderer()
{
    auto defaultCamera = std::make_unique<PlayCamera>();
    addCamera();
};

} // namespace Play