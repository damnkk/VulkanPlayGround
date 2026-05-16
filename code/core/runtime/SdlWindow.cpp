#include "SdlWindow.h"

#include <nvutils/logger.hpp>

namespace Play::runtime
{

bool SdlWindow::init(const RuntimeConfig& config)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LOGE("SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    const SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    _window                    = SDL_CreateWindow(config.windowTitle, static_cast<int>(config.width), static_cast<int>(config.height), flags);
    if (!_window)
    {
        LOGE("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    refreshPixelSize();
    _resizePending = true;
    return true;
}

void SdlWindow::deinit()
{
    if (_window)
    {
        SDL_DestroyWindow(_window);
        _window = nullptr;
    }
    SDL_Quit();
}

void SdlWindow::pollEvents()
{
    SDL_Event event{};
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            {
                _shouldClose = true;
                break;
            }
            case SDL_EVENT_KEY_DOWN:
            {
                if (event.key.key == SDLK_ESCAPE)
                {
                    _shouldClose = true;
                }
                break;
            }
            case SDL_EVENT_WINDOW_MINIMIZED:
            {
                _minimized = true;
                refreshPixelSize();
                break;
            }
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            {
                _minimized = false;
                refreshPixelSize();
                _resizePending = true;
                break;
            }
            default:
            {
                break;
            }
        }
    }
}

void SdlWindow::setTitle(const char* title)
{
    if (_window)
    {
        SDL_SetWindowTitle(_window, title);
    }
}

const char* const* SdlWindow::getVulkanInstanceExtensions(uint32_t* count) const
{
    return SDL_Vulkan_GetInstanceExtensions(count);
}

bool SdlWindow::createSurface(VkInstance instance, VkSurfaceKHR* surface) const
{
    if (!SDL_Vulkan_CreateSurface(_window, instance, nullptr, surface))
    {
        LOGE("SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void SdlWindow::refreshPixelSize()
{
    int width  = 0;
    int height = 0;
    if (_window && SDL_GetWindowSizeInPixels(_window, &width, &height))
    {
        _pixelSize = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }
    else
    {
        _pixelSize = {};
    }
}

} // namespace Play::runtime
