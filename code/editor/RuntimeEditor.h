#ifndef PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
#define PLAY_CODE_EDITOR_RUNTIMEEDITOR_H

#include "editor/EditorRegistry.h"

namespace Play
{
class RenderSession;
}

namespace Play::runtime
{
class VulkanRuntime;
}

namespace Play::editor
{

class StartupProjectReceiver
{
public:
    virtual ~StartupProjectReceiver() = default;

    virtual bool selectStartupProject(const std::string& projectPath, std::string* errorMessage) = 0;
};

class RuntimeEditor
{
public:
    RuntimeEditor();

    void bindRuntime(Play::runtime::VulkanRuntime& runtime, Play::RenderSession& renderSession, const char* activeMode);
    void setStartupProjectReceiver(StartupProjectReceiver* receiver);

    EditorRegistry& getEditorRegistry()
    {
        return _editorRegistry;
    }

    const EditorRegistry& getEditorRegistry() const
    {
        return _editorRegistry;
    }

private:
    EditorRegistry          _editorRegistry;
    StartupProjectReceiver* _startupProjectReceiver = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
