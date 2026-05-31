#ifndef PLAY_CODE_CORE_RUNTIME_RUNTIMEGUIHOST_H
#define PLAY_CODE_CORE_RUNTIME_RUNTIMEGUIHOST_H

#include <SDL3/SDL.h>

#include "editor/RuntimeEditor.h"

class QApplication;

namespace Play::editor
{
class QtRuntimeEditorWindow;
} // namespace Play::editor

namespace Play::runtime
{

class RuntimeGuiHost
{
public:
    RuntimeGuiHost();
    ~RuntimeGuiHost();

    RuntimeGuiHost(const RuntimeGuiHost&)            = delete;
    RuntimeGuiHost& operator=(const RuntimeGuiHost&) = delete;
    RuntimeGuiHost(RuntimeGuiHost&&)                 = delete;
    RuntimeGuiHost& operator=(RuntimeGuiHost&&)      = delete;

    bool start();
    void stop();

    Play::editor::RuntimeEditor& getEditor()
    {
        return _editor;
    }

    const Play::editor::RuntimeEditor& getEditor() const
    {
        return _editor;
    }

private:
    static int threadMain(void* data);

    int  run();
    void cleanupFinishedThread();
    void requestShowWindow();
    void setApplication(QApplication* application);
    void setWindow(Play::editor::QtRuntimeEditorWindow* window);
    void markThreadFinished();

    SDL_Thread*                 _thread         = nullptr;
    SDL_Mutex*                  _mutex          = nullptr;
    QApplication*               _application    = nullptr;
    Play::editor::QtRuntimeEditorWindow* _window = nullptr;
    bool                        _stopRequested  = false;
    bool                        _threadFinished = false;
    Play::editor::RuntimeEditor _editor;
};

} // namespace Play::runtime

#endif // PLAY_CODE_CORE_RUNTIME_RUNTIMEGUIHOST_H
