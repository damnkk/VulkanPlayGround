#include "RuntimeGuiHost.h"

#include <nvutils/logger.hpp>

#include "webview/webview.h"

namespace Play::runtime
{

namespace
{
constexpr const char* kRuntimeGuiHtml = R"html(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      color-scheme: dark;
      font-family: "Segoe UI", sans-serif;
      background: #111318;
      color: #f4f7fb;
    }

    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      background: linear-gradient(135deg, #111318, #1a202b);
    }

    main {
      width: min(360px, calc(100vw - 32px));
      display: grid;
      gap: 16px;
    }

    h1 {
      margin: 0;
      font-size: 20px;
      font-weight: 650;
    }

    p {
      margin: 0;
      color: #a9b4c7;
      font-size: 13px;
      line-height: 1.5;
    }

    .buttons {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }

    button {
      border: 1px solid #3c4658;
      border-radius: 6px;
      padding: 11px 12px;
      background: #202838;
      color: #f4f7fb;
      font: inherit;
      cursor: pointer;
    }

    button:hover {
      background: #2b3548;
      border-color: #56647a;
    }

    #status {
      min-height: 20px;
      color: #7cc9ff;
      font-size: 12px;
    }
  </style>
</head>
<body>
  <main>
    <h1>VulkanPlayGround UI</h1>
    <p>This companion window is independent for now.</p>
    <div class="buttons">
      <button id="button-a">Button A</button>
      <button id="button-b">Button B</button>
    </div>
    <div id="status">Ready.</div>
  </main>
  <script>
    const status = document.querySelector("#status");
    document.querySelector("#button-a").addEventListener("click", () => {
      status.textContent = "Button A clicked.";
    });
    document.querySelector("#button-b").addEventListener("click", () => {
      status.textContent = "Button B clicked.";
    });
  </script>
</body>
</html>
)html";
} // namespace

RuntimeGuiHost::~RuntimeGuiHost()
{
    stop();
}

bool RuntimeGuiHost::start()
{
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

    _stopRequested = false;
    _thread        = SDL_CreateThread(&RuntimeGuiHost::threadMain, "RuntimeGuiHost", this);
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
    _thread = nullptr;

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
        return 1;
    }

    setWebview(webview);

    SDL_LockMutex(_mutex);
    const bool shouldStop = _stopRequested;
    SDL_UnlockMutex(_mutex);

    if (!shouldStop)
    {
        webview_set_title(webview, "VulkanPlayGround Runtime UI");
        webview_set_size(webview, 420, 260, WEBVIEW_HINT_NONE);
        webview_set_html(webview, kRuntimeGuiHtml);
        webview_run(webview);
    }

    setWebview(nullptr);
    webview_destroy(webview);
    return 0;
}

void RuntimeGuiHost::setWebview(webview_t webview)
{
    SDL_LockMutex(_mutex);
    _webview = webview;
    SDL_UnlockMutex(_mutex);
}

} // namespace Play::runtime
