#ifndef PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
#define PLAY_CODE_EDITOR_RUNTIMEEDITOR_H

#include "editor/EditorProtocol.h"
#include "mutex"
namespace Play::editor
{

class RuntimeEditor
{
public:
    RuntimeEditor();

    void publishSnapshot(EditorSnapshot snapshot);
    // Copy only when the caller's snapshot is out of date.
    bool readSnapshot(EditorSnapshot& snapshot) const;

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

    // The startup page supplies a validated, non-empty UTF-8 directory once.
    void selectStartupProject(std::string projectPath, bool createNew);
    bool takeStartupProject(std::string& projectPath, bool& createNew);

private:
    mutable std::mutex                    _mutex;
    EditorSnapshot                        _snapshot;
    std::vector<EditorCommand>            _commands;
    std::vector<EditorAssetImportRequest> _assetImportRequests;
    bool                                  _exitRequested = false;
    bool                                  _saveRequested = false;
    std::string                           _startupProjectPath;
    bool                                  _startupCreateNew = false;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
