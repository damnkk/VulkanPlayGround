#ifndef PLAY_CODE_EDITOR_SCENEMANAGEREDITOR_H
#define PLAY_CODE_EDITOR_SCENEMANAGEREDITOR_H


#include "editor/EditorHtml.h"

namespace Play
{
class SceneManager;
}

namespace Play::editor
{

class SceneManagerEditor
{
public:
    void setSceneManager(Play::SceneManager* sceneManager);
    void appendHtml(std::string& html) const;

private:
    Play::SceneManager* _sceneManager = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_SCENEMANAGEREDITOR_H
