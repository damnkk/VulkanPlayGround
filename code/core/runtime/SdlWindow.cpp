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
    refreshInputState();
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
    _inputState.wheelY = 0.0F;

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
            case SDL_EVENT_MOUSE_WHEEL:
            {
                if (!_window || event.wheel.windowID == SDL_GetWindowID(_window))
                {
                    _inputState.wheelY += event.wheel.y;
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

    refreshInputState();
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

void SdlWindow::refreshInputState()
{
    float mouseX = 0.0F;
    float mouseY = 0.0F;
    const SDL_MouseButtonFlags previousMouseButtonFlags = _mouseButtonFlags;
    _mouseButtonFlags                                  = SDL_GetMouseState(&mouseX, &mouseY);

    int windowWidth  = 0;
    int windowHeight = 0;
    if (_window && SDL_GetWindowSize(_window, &windowWidth, &windowHeight) && windowWidth > 0 && windowHeight > 0)
    {
        mouseX *= static_cast<float>(_pixelSize.width) / static_cast<float>(windowWidth);
        mouseY *= static_cast<float>(_pixelSize.height) / static_cast<float>(windowHeight);
    }

    _inputState.mouseX        = mouseX;
    _inputState.mouseY        = mouseY;
    _inputState.mouseInWindow = _window && SDL_GetMouseFocus() == _window;

    _inputState.lmb        = (_mouseButtonFlags & SDL_BUTTON_LMASK) != 0;
    _inputState.mmb        = (_mouseButtonFlags & SDL_BUTTON_MMASK) != 0;
    _inputState.rmb        = (_mouseButtonFlags & SDL_BUTTON_RMASK) != 0;
    _inputState.lmbPressed = _inputState.lmb && (previousMouseButtonFlags & SDL_BUTTON_LMASK) == 0;
    _inputState.mmbPressed = _inputState.mmb && (previousMouseButtonFlags & SDL_BUTTON_MMASK) == 0;
    _inputState.rmbPressed = _inputState.rmb && (previousMouseButtonFlags & SDL_BUTTON_RMASK) == 0;

    int         keyCount = 0;
    const bool* keys     = SDL_GetKeyboardState(&keyCount);
    auto        keyDown  = [keys, keyCount](SDL_Scancode scancode)
    {
        return keys && static_cast<int>(scancode) < keyCount && keys[scancode];
    };

    _inputState.ctrl  = keyDown(SDL_SCANCODE_LCTRL) || keyDown(SDL_SCANCODE_RCTRL);
    _inputState.shift = keyDown(SDL_SCANCODE_LSHIFT) || keyDown(SDL_SCANCODE_RSHIFT);
    _inputState.alt   = keyDown(SDL_SCANCODE_LALT) || keyDown(SDL_SCANCODE_RALT);

    _inputState.keyW     = keyDown(SDL_SCANCODE_W);
    _inputState.keyA     = keyDown(SDL_SCANCODE_A);
    _inputState.keyS     = keyDown(SDL_SCANCODE_S);
    _inputState.keyD     = keyDown(SDL_SCANCODE_D);
    _inputState.keyLeft  = keyDown(SDL_SCANCODE_LEFT);
    _inputState.keyRight = keyDown(SDL_SCANCODE_RIGHT);
    _inputState.keyUp    = keyDown(SDL_SCANCODE_UP);
    _inputState.keyDown  = keyDown(SDL_SCANCODE_DOWN);
}

} // namespace Play::runtime
