#include "RuntimeGuiHost.h"

#include <string>

#include <nvutils/logger.hpp>

#include "webview/webview.h"
#include "webview/detail/json.hh"

namespace Play::runtime
{

namespace
{
bool parseEditorObjectId(const std::string& text, Play::editor::EditorObjectId& id)
{
    Play::editor::EditorObjectId parsedId = 0;
    for (const char ch : text)
    {
        if (ch < '0' || ch > '9')
        {
            return false;
        }

        parsedId = parsedId * 10 + static_cast<Play::editor::EditorObjectId>(ch - '0');
    }

    id = parsedId;
    return id != 0;
}

std::string makeJsonString(const std::string& value)
{
    std::string json = "\"";
    for (const char ch : value)
    {
        switch (ch)
        {
            case '\\':
                json += "\\\\";
                break;
            case '"':
                json += "\\\"";
                break;
            case '\n':
                json += "\\n";
                break;
            case '\r':
                json += "\\r";
                break;
            case '\t':
                json += "\\t";
                break;
            default:
                json += ch;
                break;
        }
    }
    json += "\"";
    return json;
}
} // namespace

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

void RuntimeGuiHost::setEditorProperty(const char* id, const char* request, void* arg)
{
    RuntimeGuiHost* host = static_cast<RuntimeGuiHost*>(arg);
    if (!host || !host->_webview)
    {
        return;
    }

    const std::string requestJson  = request ? request : "";
    const std::string objectIdText = webview::detail::json_parse(requestJson, "", 0);
    const std::string propertyName = webview::detail::json_parse(requestJson, "", 1);
    const std::string value        = webview::detail::json_parse(requestJson, "", 2);

    Play::editor::EditorObjectId objectId = 0;
    const bool parsedId = parseEditorObjectId(objectIdText, objectId);
    const bool changed  = parsedId && !propertyName.empty()
                          && host->_editor.getEditorRegistry().setObjectProperty(objectId, propertyName.c_str(), rttr::variant(value));

    webview_return(host->_webview, id, changed ? 0 : 1, changed ? "true" : "false");
}

void RuntimeGuiHost::resetEditorObject(const char* id, const char* request, void* arg)
{
    RuntimeGuiHost* host = static_cast<RuntimeGuiHost*>(arg);
    if (!host || !host->_webview)
    {
        return;
    }

    const std::string requestJson  = request ? request : "";
    const std::string objectIdText = webview::detail::json_parse(requestJson, "", 0);

    Play::editor::EditorObjectId objectId = 0;
    const bool parsedId = parseEditorObjectId(objectIdText, objectId);
    const bool changed  = parsedId && host->_editor.getEditorRegistry().resetObject(objectId);

    webview_return(host->_webview, id, changed ? 0 : 1, changed ? "true" : "false");
}

void RuntimeGuiHost::createSceneNode(const char* id, const char* request, void* arg)
{
    RuntimeGuiHost* host = static_cast<RuntimeGuiHost*>(arg);
    if (!host || !host->_webview)
    {
        return;
    }

    const std::string requestJson   = request ? request : "";
    const std::string renderModeId  = webview::detail::json_parse(requestJson, "", 0);
    const std::string parentNodeKey = webview::detail::json_parse(requestJson, "", 1);
    const std::string nodeType      = webview::detail::json_parse(requestJson, "", 2);

    const std::string nodeKey = host->_editor.getRenderModeTabs().createSceneNode(renderModeId.c_str(), parentNodeKey.c_str(), nodeType.c_str());
    const std::string result  = makeJsonString(nodeKey);
    webview_return(host->_webview, id, nodeKey.empty() ? 1 : 0, result.c_str());
}

void RuntimeGuiHost::setSceneNodeTransform(const char* id, const char* request, void* arg)
{
    RuntimeGuiHost* host = static_cast<RuntimeGuiHost*>(arg);
    if (!host || !host->_webview)
    {
        return;
    }

    const std::string requestJson   = request ? request : "";
    const std::string renderModeId  = webview::detail::json_parse(requestJson, "", 0);
    const std::string nodeKey       = webview::detail::json_parse(requestJson, "", 1);
    const std::string transformPath = webview::detail::json_parse(requestJson, "", 2);
    const std::string value         = webview::detail::json_parse(requestJson, "", 3);

    const bool changed = host->_editor.getRenderModeTabs().setSceneNodeTransform(renderModeId.c_str(), nodeKey.c_str(), transformPath.c_str(), value.c_str());
    webview_return(host->_webview, id, changed ? 0 : 1, changed ? "true" : "false");
}

void RuntimeGuiHost::addSceneNodeComponent(const char* id, const char* request, void* arg)
{
    RuntimeGuiHost* host = static_cast<RuntimeGuiHost*>(arg);
    if (!host || !host->_webview)
    {
        return;
    }

    const std::string requestJson  = request ? request : "";
    const std::string renderModeId = webview::detail::json_parse(requestJson, "", 0);
    const std::string nodeKey      = webview::detail::json_parse(requestJson, "", 1);
    const std::string component    = webview::detail::json_parse(requestJson, "", 2);

    const bool changed = host->_editor.getRenderModeTabs().addSceneNodeComponent(renderModeId.c_str(), nodeKey.c_str(), component.c_str());
    webview_return(host->_webview, id, changed ? 0 : 1, changed ? "true" : "false");
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
        webview_bind(webview, "setEditorProperty", &RuntimeGuiHost::setEditorProperty, this);
        webview_bind(webview, "resetEditorObject", &RuntimeGuiHost::resetEditorObject, this);
        webview_bind(webview, "createSceneNode", &RuntimeGuiHost::createSceneNode, this);
        webview_bind(webview, "setSceneNodeTransform", &RuntimeGuiHost::setSceneNodeTransform, this);
        webview_bind(webview, "addSceneNodeComponent", &RuntimeGuiHost::addSceneNodeComponent, this);
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
