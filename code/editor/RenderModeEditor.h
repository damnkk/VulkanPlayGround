#pragma once

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

    void setSceneManager(Play::SceneManager* sceneManager);

    void appendTabHtml(std::string& html, bool active) const;
    void appendPageHtml(std::string& html, bool active) const;

private:
    std::string        _id;
    std::string        _title;
    SceneManagerEditor _sceneManagerEditor;
    ControlPanel       _controlPanel;
};

} // namespace Play::editor
