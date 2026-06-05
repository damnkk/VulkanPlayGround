#ifndef PLAY_CODE_EDITOR_RENDERMODEEDITOR_H
#define PLAY_CODE_EDITOR_RENDERMODEEDITOR_H

#include "editor/ControlPanel.h"
#include "editor/SceneManagerEditor.h"

namespace Play
{
class SceneManager;
}

namespace Play::editor
{

class RenderModeEditor
{
public:
    RenderModeEditor(const char* id, const char* title, EditorRegistry& editorRegistry, EditorRenderMode renderMode);

    const char* getId() const;
    const char* getTitle() const;

    void        setSceneManager(Play::SceneManager* sceneManager);
    void        buildSnapshot(EditorUiRenderMode& renderMode, bool active) const;
    std::string createSceneNode(const char* parentNodeKey, const char* nodeType);
    bool        setSceneNodeTransform(const char* nodeKey, const char* transformPath, const char* value);
    bool        addSceneNodeComponent(const char* nodeKey, const char* componentType);
    bool        loadSceneNodeModel(const char* nodeKey, const char* path);

private:
    std::string        _id;
    std::string        _title;
    SceneManagerEditor _sceneManagerEditor;
    ControlPanel       _controlPanel;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_RENDERMODEEDITOR_H
