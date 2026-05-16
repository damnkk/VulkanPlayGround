#pragma once

#include "RuntimeConfig.h"
#include "vulkan/vulkan.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace Play::runtime
{

class SdlWindow
{
public:
    bool init(const RuntimeConfig& config);
    void deinit();

    void pollEvents();

    bool shouldClose() const
    {
        return _shouldClose;
    }

    bool isCreated() const
    {
        return _window != nullptr;
    }

    bool isRenderable() const
    {
        return !_minimized && _pixelSize.width > 0 && _pixelSize.height > 0;
    }

    bool consumeResizePending()
    {
        const bool pending = _resizePending;
        _resizePending    = false;
        return pending;
    }

    const VkExtent2D& getPixelSize() const
    {
        return _pixelSize;
    }

    void setTitle(const char* title);

    const char* const* getVulkanInstanceExtensions(uint32_t* count) const;
    bool               createSurface(VkInstance instance, VkSurfaceKHR* surface) const;

private:
    void refreshPixelSize();

    SDL_Window* _window        = nullptr;
    VkExtent2D  _pixelSize     = {};
    bool        _shouldClose   = false;
    bool        _minimized     = false;
    bool        _resizePending = false;
};

} // namespace Play::runtime
