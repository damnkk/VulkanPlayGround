#include "editor/RenderModeEditor.h"

namespace Play::editor
{

RenderModeEditor::RenderModeEditor(const char* id, const char* title, EditorRegistry& editorRegistry, EditorRenderMode renderMode)
    : _id(id ? id : ""), _title(title ? title : ""), _sceneManagerEditor(id), _controlPanel(editorRegistry, renderMode)
{
}

const char* RenderModeEditor::getId() const
{
    return _id.c_str();
}

const char* RenderModeEditor::getTitle() const
{
    return _title.c_str();
}

void RenderModeEditor::setSceneManager(Play::SceneManager* sceneManager)
{
    _sceneManagerEditor.setSceneManager(sceneManager);
}

void RenderModeEditor::buildSnapshot(EditorUiRenderMode& renderMode, bool active) const
{
    renderMode.id     = _id;
    renderMode.title  = _title;
    renderMode.active = active;

    _sceneManagerEditor.buildSnapshot(renderMode);
    _controlPanel.buildSnapshot(renderMode);
}

std::string RenderModeEditor::createSceneNode(const char* parentNodeKey, const char* nodeType)
{
    return _sceneManagerEditor.createSceneNode(parentNodeKey, nodeType);
}

bool RenderModeEditor::setSceneNodeTransform(const char* nodeKey, const char* transformPath, const char* value)
{
    return _sceneManagerEditor.setSceneNodeTransform(nodeKey, transformPath, value);
}

bool RenderModeEditor::addSceneNodeComponent(const char* nodeKey, const char* componentType)
{
    return _sceneManagerEditor.addSceneNodeComponent(nodeKey, componentType);
}

} // namespace Play::editor
