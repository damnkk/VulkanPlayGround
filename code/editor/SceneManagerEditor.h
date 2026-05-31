#ifndef PLAY_CODE_EDITOR_SCENEMANAGEREDITOR_H
#define PLAY_CODE_EDITOR_SCENEMANAGEREDITOR_H

#include "editor/EditorUiModel.h"

namespace Play
{
class SceneManager;
}

namespace Play::editor
{

class SceneManagerEditor
{
public:
    explicit SceneManagerEditor(const char* renderModeId = nullptr);

    void        setSceneManager(Play::SceneManager* sceneManager);
    void        buildSnapshot(EditorUiRenderMode& renderMode) const;
    std::string createSceneNode(const char* parentNodeKey, const char* nodeType);
    bool        setSceneNodeTransform(const char* nodeKey, const char* transformPath, const char* value);
    bool        addSceneNodeComponent(const char* nodeKey, const char* componentType);

private:
    Play::SceneManager* _sceneManager = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_SCENEMANAGEREDITOR_H
