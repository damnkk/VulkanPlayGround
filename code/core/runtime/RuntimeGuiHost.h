#pragma once

#include <SDL3/SDL.h>

#include "webview/types.h"

namespace Play::runtime
{

class RuntimeGuiHost
{
public:
    RuntimeGuiHost() = default;
    ~RuntimeGuiHost();

    RuntimeGuiHost(const RuntimeGuiHost&)            = delete;
    RuntimeGuiHost& operator=(const RuntimeGuiHost&) = delete;
    RuntimeGuiHost(RuntimeGuiHost&&)                 = delete;
    RuntimeGuiHost& operator=(RuntimeGuiHost&&)      = delete;

    bool start();
    void stop();

private:
    static int  threadMain(void* data);
    static void requestTerminate(webview_t webview, void* arg);

    int  run();
    void setWebview(webview_t webview);

    SDL_Thread* _thread        = nullptr;
    SDL_Mutex*  _mutex         = nullptr;
    webview_t   _webview       = nullptr;
    bool        _stopRequested = false;
};

} // namespace Play::runtime

