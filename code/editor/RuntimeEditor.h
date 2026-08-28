#ifndef PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
#define PLAY_CODE_EDITOR_RUNTIMEEDITOR_H

#include "editor/EditorRegistry.h"
#include "editor/EditorRuntimeContext.h"
#include "editor/RenderModeTabs.h"

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

    bool isRuntimeBound() const
    {
        return _runtimeContext.getRuntime() != nullptr;
    }

    EditorRegistry& getEditorRegistry()
    {
        return _editorRegistry;
    }

    const EditorRegistry& getEditorRegistry() const
    {
        return _editorRegistry;
    }

    RenderModeTabs& getRenderModeTabs()
    {
        return _renderModeTabs;
    }

    EditorUiSnapshot buildSnapshot() const;

    bool createProject(const std::string& projectPath, std::string* errorMessage = nullptr);
    bool saveProject(std::string* errorMessage = nullptr);
    bool loadProject(const std::string& projectPath, std::string* errorMessage = nullptr);

private:
    EditorRuntimeContext    _runtimeContext;
    EditorRegistry          _editorRegistry;
    RenderModeTabs          _renderModeTabs;
    StartupProjectReceiver* _startupProjectReceiver = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
