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

EditorSnapshot RuntimeEditor::getSnapshot() const
{
    std::lock_guard lock(_mutex);
    return _snapshot;
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

void RuntimeEditor::setStartupProjectReceiver(StartupProjectReceiver* receiver)
{
    _startupProjectReceiver = receiver;
}

StartupProjectReceiver* RuntimeEditor::getStartupProjectReceiver() const
{
    return _startupProjectReceiver;
}

} // namespace Play::editor
