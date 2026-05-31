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

class RuntimeEditor
{
public:
    RuntimeEditor();

    void bindRuntime(Play::runtime::VulkanRuntime& runtime, Play::RenderSession& renderSession, const char* activeMode);

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

private:
    EditorRuntimeContext _runtimeContext;
    EditorRegistry       _editorRegistry;
    RenderModeTabs       _renderModeTabs;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_RUNTIMEEDITOR_H
