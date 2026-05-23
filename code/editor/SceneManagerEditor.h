#pragma once

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
