#include "RuntimeGuiHost.h"

#include <QApplication>
#include <QMetaObject>
#include <QMessageBox>

#include <nvutils/logger.hpp>

#include "editor/QtRuntimeEditorWindow.h"

namespace Play::runtime
{

RuntimeGuiHost::RuntimeGuiHost()
{
    _editor.setStartupProjectReceiver(this);
}

RuntimeGuiHost::~RuntimeGuiHost()
{
    stop();
}

bool RuntimeGuiHost::start()
{
    cleanupFinishedThread();

    if (_thread)
    {
        requestShowWindow();
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
    _startupProjectPath.clear();
    _startupProjectSelected = false;
    _thread                 = SDL_CreateThread(&RuntimeGuiHost::threadMain, "RuntimeGuiHost", this);
    if (!_thread)
    {
        LOGE("RuntimeGuiHost: SDL_CreateThread failed: %s\n", SDL_GetError());
        SDL_DestroyMutex(_mutex);
        _mutex = nullptr;
        return false;
    }

    return true;
}

bool RuntimeGuiHost::selectStartupProject(const std::string& projectPath, bool createNew, std::string* errorMessage)
{
    if (!_mutex)
    {
        if (errorMessage)
        {
            *errorMessage = "Project startup is not available.";
        }
        return false;
    }

    SDL_LockMutex(_mutex);
    const bool isStopping = _stopRequested;
    if (!isStopping)
    {
        _startupProjectPath     = projectPath;
        _startupCreateNew       = createNew;
        _startupProjectSelected = true;
    }
    SDL_UnlockMutex(_mutex);

    if (isStopping && errorMessage)
    {
        *errorMessage = "Project startup has already stopped.";
    }
    return !isStopping;
}

void RuntimeGuiHost::showError(const std::string& message)
{
    SDL_LockMutex(_mutex);
    auto* window = _window;
    SDL_UnlockMutex(_mutex);
    if (!window) return;
    // Keep Qt and the hidden viewport alive until the user has read the error.
    QMetaObject::invokeMethod(
        window, [window, message]() { QMessageBox::critical(window, "Project startup failed", QString::fromStdString(message)); },
        Qt::BlockingQueuedConnection);
}

void RuntimeGuiHost::setRenderWindowHandle(void* handle)
{
    Play::editor::QtRuntimeEditorWindow* window = nullptr;
    SDL_LockMutex(_mutex);
    _renderWindowHandle = handle;
    window              = _window;
    SDL_UnlockMutex(_mutex);

    if (window)
    {
        QMetaObject::invokeMethod(window, [window, handle]() { window->setRenderWindowHandle(handle); }, Qt::QueuedConnection);
    }
}

bool RuntimeGuiHost::takeStartupProject(std::string& projectPath, bool& createNew)
{
    if (!_mutex)
    {
        return false;
    }

    SDL_LockMutex(_mutex);
    if (!_startupProjectSelected)
    {
        SDL_UnlockMutex(_mutex);
        return false;
    }

    projectPath = _startupProjectPath;
    createNew   = _startupCreateNew;
    _startupProjectPath.clear();
    _startupProjectSelected = false;
    SDL_UnlockMutex(_mutex);
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

    QApplication* application = nullptr;
    SDL_LockMutex(_mutex);
    _stopRequested = true;
    application    = _application;
    SDL_UnlockMutex(_mutex);

    if (application)
    {
        QMetaObject::invokeMethod(application, "quit", Qt::QueuedConnection);
    }

    for (;;)
    {
        SDL_LockMutex(_mutex);
        const bool threadFinished = _threadFinished;
        SDL_UnlockMutex(_mutex);

        if (threadFinished)
        {
            break;
        }

        SDL_PumpEvents();
        SDL_Delay(1);
    }

    int threadStatus = 0;
    SDL_WaitThread(_thread, &threadStatus);
    _thread         = nullptr;
    _threadFinished = false;

    SDL_LockMutex(_mutex);
    _application = nullptr;
    _window      = nullptr;
    SDL_UnlockMutex(_mutex);

    SDL_DestroyMutex(_mutex);
    _mutex = nullptr;
}

int RuntimeGuiHost::threadMain(void* data)
{
    return static_cast<RuntimeGuiHost*>(data)->run();
}

int RuntimeGuiHost::run()
{
    char  applicationName[] = "VulkanPlayGroundEditor";
    char* arguments[]       = {applicationName, nullptr};
    int   argumentCount     = 1;

    QApplication application(argumentCount, arguments);
    application.setQuitOnLastWindowClosed(false);
    setApplication(&application);

    SDL_LockMutex(_mutex);
    const bool shouldStop = _stopRequested;
    SDL_UnlockMutex(_mutex);

    int result = 0;
    if (!shouldStop)
    {
        Play::editor::QtRuntimeEditorWindow window(_editor);
        setWindow(&window);
        window.show();
        result = application.exec();
        // Also cover exits initiated by SDL (Escape or a native close request).
        // Hide the foreign window before Qt releases its container and reparents it.
        window.hide();
        setWindow(nullptr);
    }

    setApplication(nullptr);
    _editor.requestExit();
    markThreadFinished();
    return result;
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

void RuntimeGuiHost::requestShowWindow()
{
    Play::editor::QtRuntimeEditorWindow* window = nullptr;
    SDL_LockMutex(_mutex);
    window = _window;
    SDL_UnlockMutex(_mutex);

    if (!window)
    {
        return;
    }

    QMetaObject::invokeMethod(
        window,
        [window]()
        {
            window->showNormal();
            window->raise();
            window->activateWindow();
        },
        Qt::QueuedConnection);
}

void RuntimeGuiHost::setApplication(QApplication* application)
{
    SDL_LockMutex(_mutex);
    _application = application;
    SDL_UnlockMutex(_mutex);
}

void RuntimeGuiHost::setWindow(Play::editor::QtRuntimeEditorWindow* window)
{
    SDL_LockMutex(_mutex);
    _window                  = window;
    void* renderWindowHandle = _renderWindowHandle;
    SDL_UnlockMutex(_mutex);

    if (window && renderWindowHandle)
    {
        window->setRenderWindowHandle(renderWindowHandle);
    }
}

void RuntimeGuiHost::markThreadFinished()
{
    SDL_LockMutex(_mutex);
    _threadFinished = true;
    SDL_UnlockMutex(_mutex);
}

} // namespace Play::runtime
