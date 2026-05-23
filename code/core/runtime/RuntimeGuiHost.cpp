#include "RuntimeGuiHost.h"

#include <string>

#include <nvutils/logger.hpp>

#include "webview/webview.h"

namespace Play::runtime
{

RuntimeGuiHost::RuntimeGuiHost() = default;

RuntimeGuiHost::~RuntimeGuiHost()
{
    stop();
}

bool RuntimeGuiHost::start()
{
    cleanupFinishedThread();

    if (_thread)
    {
        return true;
    }

    _mutex = SDL_CreateMutex();
    if (!_mutex)
    {
        LOGE("RuntimeGuiHost: SDL_CreateMutex failed: %s\n", SDL_GetError());
        return false;
    }

    _stopRequested  = false;
    _threadFinished = false;
    _thread         = SDL_CreateThread(&RuntimeGuiHost::threadMain, "RuntimeGuiHost", this);
    if (!_thread)
    {
        LOGE("RuntimeGuiHost: SDL_CreateThread failed: %s\n", SDL_GetError());
        SDL_DestroyMutex(_mutex);
        _mutex = nullptr;
        return false;
    }

    return true;
}

void RuntimeGuiHost::stop()
{
    if (!_thread)
    {
        if (_mutex)
        {
            SDL_DestroyMutex(_mutex);
            _mutex = nullptr;
        }
        return;
    }

    SDL_LockMutex(_mutex);
    _stopRequested = true;
    if (_webview)
    {
        webview_dispatch(_webview, &RuntimeGuiHost::requestTerminate, nullptr);
    }
    SDL_UnlockMutex(_mutex);

    int threadStatus = 0;
    SDL_WaitThread(_thread, &threadStatus);
    _thread         = nullptr;
    _threadFinished = false;

    SDL_LockMutex(_mutex);
    _webview = nullptr;
    SDL_UnlockMutex(_mutex);

    SDL_DestroyMutex(_mutex);
    _mutex = nullptr;
}

int RuntimeGuiHost::threadMain(void* data)
{
    return static_cast<RuntimeGuiHost*>(data)->run();
}

void RuntimeGuiHost::requestTerminate(webview_t webview, void* arg)
{
    (void) arg;
    webview_terminate(webview);
}

int RuntimeGuiHost::run()
{
    webview_t webview = webview_create(0, nullptr);
    if (!webview)
    {
        LOGE("RuntimeGuiHost: webview_create failed. WebView2 runtime may be unavailable.\n");
        markThreadFinished();
        return 1;
    }

    setWebview(webview);

    SDL_LockMutex(_mutex);
    const bool shouldStop = _stopRequested;
    SDL_UnlockMutex(_mutex);

    if (!shouldStop)
    {
        const std::string html = _editor.buildHtml();

        webview_set_title(webview, "VulkanPlayGround Runtime UI");
        webview_set_size(webview, 920, 640, WEBVIEW_HINT_NONE);
        webview_set_html(webview, html.c_str());
        webview_run(webview);
    }

    setWebview(nullptr);
    webview_destroy(webview);
    markThreadFinished();
    return 0;
}

void RuntimeGuiHost::cleanupFinishedThread()
{
    if (!_thread)
    {
        return;
    }

    SDL_LockMutex(_mutex);
    const bool threadFinished = _threadFinished;
    SDL_UnlockMutex(_mutex);

    if (!threadFinished)
    {
        return;
    }

    int threadStatus = 0;
    SDL_WaitThread(_thread, &threadStatus);
    _thread         = nullptr;
    _threadFinished = false;

    SDL_DestroyMutex(_mutex);
    _mutex = nullptr;
}

void RuntimeGuiHost::setWebview(webview_t webview)
{
    SDL_LockMutex(_mutex);
    _webview = webview;
    SDL_UnlockMutex(_mutex);
}

void RuntimeGuiHost::markThreadFinished()
{
    SDL_LockMutex(_mutex);
    _threadFinished = true;
    SDL_UnlockMutex(_mutex);
}

} // namespace Play::runtime
