#include "editor/RuntimeEditor.h"

namespace Play::editor
{

RuntimeEditor::RuntimeEditor() : _renderModeTabs(_runtimeContext, _editorRegistry)
{
    _renderModeTabs.addRenderMode("defer", "Defer", EditorRenderMode::Defer);
    _renderModeTabs.addRenderMode("gaussian", "Gaussian", EditorRenderMode::Gaussian);
    _renderModeTabs.addRenderMode("raytrace", "Ray Tracing", EditorRenderMode::Raytrace);
    _renderModeTabs.addRenderMode("volume", "Volume", EditorRenderMode::Volume);
}

void RuntimeEditor::bindRuntime(Play::runtime::VulkanRuntime& runtime, Play::RenderSession& renderSession, const char* activeMode)
{
    _runtimeContext.bind(runtime, renderSession, _editorRegistry);
    _renderModeTabs.bindRenderSession(renderSession, activeMode);
}

EditorUiSnapshot RuntimeEditor::buildSnapshot() const
{
    EditorUiSnapshot snapshot;
    _renderModeTabs.buildSnapshot(snapshot);
    return snapshot;
}

} // namespace Play::editor
