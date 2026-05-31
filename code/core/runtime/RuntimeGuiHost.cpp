#include "RuntimeGuiHost.h"

#include <QApplication>
#include <QMetaObject>

#include <nvutils/logger.hpp>

#include "editor/QtRuntimeEditorWindow.h"

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

    QApplication* application = nullptr;
    SDL_LockMutex(_mutex);
    _stopRequested = true;
    application    = _application;
    SDL_UnlockMutex(_mutex);

    if (application)
    {
        QMetaObject::invokeMethod(application, "quit", Qt::QueuedConnection);
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
    char  applicationName[] = "VulkanPlayGroundControlPanel";
    char* arguments[]       = {applicationName, nullptr};
    int   argumentCount     = 1;

    QApplication application(argumentCount, arguments);
    // Closing the panel should hide it, not tear down QApplication. Recreating QApplication in this SDL thread can trip Qt platform pixmap state.
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
        setWindow(nullptr);
    }

    setApplication(nullptr);
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

    QMetaObject::invokeMethod(window,
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
    _window = window;
    SDL_UnlockMutex(_mutex);
}

void RuntimeGuiHost::markThreadFinished()
{
    SDL_LockMutex(_mutex);
    _threadFinished = true;
    SDL_UnlockMutex(_mutex);
}

} // namespace Play::runtime
