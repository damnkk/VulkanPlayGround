#include "editor/RuntimeEditor.h"
#include "memory"
#include "mutex"
namespace Play::editor
{

RuntimeEditor::RuntimeEditor() = default;

void RuntimeEditor::publishSnapshot(EditorSnapshot snapshot)
{
    std::lock_guard lock(_mutex);
    snapshot.revision = _snapshot.revision + 1;
    _snapshot         = std::move(snapshot);
}

bool RuntimeEditor::readSnapshot(EditorSnapshot& snapshot) const
{
    std::lock_guard lock(_mutex);
    if (snapshot.revision == _snapshot.revision) return false;
    snapshot = _snapshot;
    return true;
}

void RuntimeEditor::submit(EditorCommand command)
{
    std::lock_guard lock(_mutex);
    _commands.push_back(std::move(command));
}

std::vector<EditorCommand> RuntimeEditor::takeCommands()
{
    std::lock_guard            lock(_mutex);
    std::vector<EditorCommand> commands;
    commands.swap(_commands);
    return commands;
}

void RuntimeEditor::requestExit()
{
    std::lock_guard lock(_mutex);
    _exitRequested = true;
}

bool RuntimeEditor::exitRequested() const
{
    std::lock_guard lock(_mutex);
    return _exitRequested;
}

void RuntimeEditor::requestSave()
{
    std::lock_guard lock(_mutex);
    _saveRequested = true;
}

bool RuntimeEditor::takeSaveRequest()
{
    std::lock_guard lock(_mutex);
    // The engine calls this after applying its drained command/import batches.
    // Edits arriving after those batches were taken must run before the save.
    if (!_commands.empty() || !_assetImportRequests.empty()) return false;
    const bool requested = _saveRequested;
    _saveRequested       = false;
    return requested;
}

void RuntimeEditor::requestAssetImport(EditorAssetImportRequest request)
{
    std::lock_guard lock(_mutex);
    _assetImportRequests.push_back(std::move(request));
}

std::vector<EditorAssetImportRequest> RuntimeEditor::takeAssetImportRequests()
{
    std::lock_guard                       lock(_mutex);
    std::vector<EditorAssetImportRequest> requests;
    requests.swap(_assetImportRequests);
    return requests;
}

void RuntimeEditor::selectStartupProject(std::string projectPath, bool createNew)
{
    std::lock_guard lock(_mutex);
    _startupProjectPath = std::move(projectPath);
    _startupCreateNew   = createNew;
}

bool RuntimeEditor::takeStartupProject(std::string& projectPath, bool& createNew)
{
    std::lock_guard lock(_mutex);
    if (_startupProjectPath.empty()) return false;
    projectPath = std::move(_startupProjectPath);
    _startupProjectPath.clear();
    createNew = _startupCreateNew;
    return true;
}

} // namespace Play::editor
