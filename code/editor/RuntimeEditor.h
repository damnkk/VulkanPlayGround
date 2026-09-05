#ifndef PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
#define PLAY_CODE_EDITOR_RUNTIMEEDITOR_H

#include "editor/EditorProtocol.h"
#include "mutex"
namespace Play::editor
{

class StartupProjectReceiver
{
public:
    virtual ~StartupProjectReceiver() = default;

    // UTF-8 base directory for loading; an empty path starts a new project.
    virtual bool selectStartupProject(const std::string& projectPath, std::string* errorMessage) = 0;
};

class RuntimeEditor
{
public:
    RuntimeEditor();

    void           publishSnapshot(EditorSnapshot snapshot);
    EditorSnapshot getSnapshot() const;

    void                       submit(EditorCommand command);
    std::vector<EditorCommand> takeCommands();

    void requestExit();
    bool exitRequested() const;

    void                    setStartupProjectReceiver(StartupProjectReceiver* receiver);
    StartupProjectReceiver* getStartupProjectReceiver() const;

private:
    mutable std::mutex         _mutex;
    EditorSnapshot             _snapshot;
    std::vector<EditorCommand> _commands;
    bool                       _exitRequested          = false;
    StartupProjectReceiver*    _startupProjectReceiver = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
