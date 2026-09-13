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

    // UTF-8 base directory for either a new or an existing project; must not be empty.
    virtual bool selectStartupProject(const std::string& projectPath, bool createNew, std::string* errorMessage) = 0;
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

    // UI save requests (File > Save, the toolbar button, or Ctrl+S) set a
    // one-shot flag that the engine loop consumes through takeSaveRequest().
    // Saving stays off the scene-command path because a save locks the scene.
    void requestSave();
    bool takeSaveRequest();

    void                                  requestAssetImport(EditorAssetImportRequest request);
    std::vector<EditorAssetImportRequest> takeAssetImportRequests();

    void                    setStartupProjectReceiver(StartupProjectReceiver* receiver);
    StartupProjectReceiver* getStartupProjectReceiver() const;

private:
    mutable std::mutex                    _mutex;
    EditorSnapshot                        _snapshot;
    std::vector<EditorCommand>            _commands;
    std::vector<EditorAssetImportRequest> _assetImportRequests;
    bool                                  _exitRequested          = false;
    bool                                  _saveRequested          = false;
    StartupProjectReceiver*               _startupProjectReceiver = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
