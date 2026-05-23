#ifndef PLAY_CODE_CORE_RUNTIME_SDLWINDOW_H
#define PLAY_CODE_CORE_RUNTIME_SDLWINDOW_H


#include "RuntimeConfig.h"
#include "vulkan/vulkan.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

namespace Play::runtime
{

struct SdlInputState
{
    float mouseX = 0.0F;
    float mouseY = 0.0F;
    float wheelY = 0.0F;

    bool mouseInWindow = false;
    bool lmb           = false;
    bool mmb           = false;
    bool rmb           = false;
    bool lmbPressed    = false;
    bool mmbPressed    = false;
    bool rmbPressed    = false;

    bool ctrl  = false;
    bool shift = false;
    bool alt   = false;

    bool keyW     = false;
    bool keyA     = false;
    bool keyS     = false;
    bool keyD     = false;
    bool keyO        = false;
    bool keyOPressed = false;
    bool keyLeft  = false;
    bool keyRight = false;
    bool keyUp    = false;
    bool keyDown  = false;
};

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

    const SdlInputState& getInputState() const
    {
        return _inputState;
    }

    void setTitle(const char* title);

    const char* const* getVulkanInstanceExtensions(uint32_t* count) const;
    bool               createSurface(VkInstance instance, VkSurfaceKHR* surface) const;

private:
    void refreshPixelSize();
    void refreshInputState();

    SDL_Window*          _window             = nullptr;
    VkExtent2D           _pixelSize          = {};
    SdlInputState        _inputState         = {};
    SDL_MouseButtonFlags _mouseButtonFlags   = 0;
    bool                 _shouldClose        = false;
    bool                 _minimized          = false;
    bool                 _resizePending      = false;
};

} // namespace Play::runtime

#endif // PLAY_CODE_CORE_RUNTIME_SDLWINDOW_H
