#include "editor/EditorRuntimeContext.h"

namespace Play::editor
{

void EditorRuntimeContext::bind(Play::runtime::VulkanRuntime& runtime, Play::RenderSession& renderSession, EditorRegistry& editorRegistry)
{
    _runtime        = &runtime;
    _renderSession  = &renderSession;
    _editorRegistry = &editorRegistry;
}

bool EditorRuntimeContext::requestRenderMode(const char* renderMode)
{
    (void) renderMode;
    return false;
}

bool EditorRuntimeContext::requestNewProject()
{
    return false;
}

bool EditorRuntimeContext::requestOpenProject()
{
    return false;
}

bool EditorRuntimeContext::requestSaveProject()
{
    return false;
}

} // namespace Play::editor
