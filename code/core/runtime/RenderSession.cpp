#include "RenderSession.h"

#include "DeferRendering.h"
#include "GaussianRenderer.h"
#include "VulkanDriver.h"

#include <nvutils/logger.hpp>

namespace Play
{

RenderSession::RenderSession(Info info) : _info(info)
{
    const std::string& mode = _info.renderMode;
    if (mode == "raster")
    {
        _renderMode = eRasterization;
    }
    else if (mode == "raytrace")
    {
        _renderMode = eRayTracing;
    }
    else if (mode == "volume")
    {
        _renderMode = eVolumeRendering;
    }
    else if (mode == "shadingrate")
    {
        _renderMode = eShadingRateRendering;
    }
    else if (mode == "defer")
    {
        _renderMode = eDeferRendering;
    }
    else if (mode == "gaussian")
    {
        _renderMode = eGaussianRendering;
    }
}

RenderSession::~RenderSession()
{
    destroy();
}

bool RenderSession::init()
{
    if (_initialized)
    {
        return true;
    }

    switch (_renderMode)
    {
        case eDeferRendering:
        {
            _renderer = std::make_unique<DeferRenderer>(*this);
            break;
        }
        case eGaussianRendering:
        {
            _renderer = std::make_unique<GaussianRenderer>(*this);
            break;
        }
        default:
        {
            LOGE("Unsupported render mode, defaulting to DeferRendering");
            _renderer = std::make_unique<DeferRenderer>(*this);
            break;
        }
    }

    if (vkDriver)
    {
        onResize(vkDriver->getViewportSize());
    }

    _initialized = true;
    return true;
}

void RenderSession::destroy()
{
    if (!_initialized && !_renderer)
    {
        return;
    }

    if (vkDriver && vkDriver->getGfxQueue().queue != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(vkDriver->getDevice());
    }

    _renderer.reset();
    _initialized = false;
}

void RenderSession::onResize(const VkExtent2D& size)
{
    if (!_renderer)
    {
        return;
    }

    if (vkDriver && vkDriver->getDevice() != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(vkDriver->getDevice());
    }

    _renderer->OnResize(size.width, size.height);
}

void RenderSession::beginFrame()
{
    if (!_renderer)
    {
        return;
    }

    _renderer->OnPreRender();
}

void RenderSession::renderFrame()
{
    if (!_renderer || !vkDriver)
    {
        return;
    }

    const VkExtent2D& viewportSize = vkDriver->getViewportSize();
    if (viewportSize.width == 0 || viewportSize.height == 0)
    {
        return;
    }

    _renderer->RenderFrame();
    _renderer->OnPostRender();
}

SceneManager* RenderSession::getSceneManager()
{
    return _renderer ? _renderer->getSceneManager() : nullptr;
}

} // namespace Play
