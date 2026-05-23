#include "editor/SceneManagerEditor.h"

#include "editor/EditorHtml.h"
#include "resourceManagement/SceneManager.h"

namespace Play::editor
{

void SceneManagerEditor::setSceneManager(Play::SceneManager* sceneManager)
{
    _sceneManager = sceneManager;
}

void SceneManagerEditor::appendHtml(std::string& html) const
{
    html += "<section class=\"scene-manager-editor\"><section class=\"panel scene-tree\"><h2>Scene Tree</h2>";
    if (!_sceneManager)
    {
        html += "<div class=\"empty\">No SceneManager for this render mode.</div>";
        html += "</section><section class=\"panel inspector\"><h2>Inspector</h2><div class=\"empty\">No scene selection.</div></section></section>";
        return;
    }

    html += "<button class=\"scene-root selected\"><span>SceneManager</span></button>";
    html += "</section><section class=\"panel inspector\"><h2>Inspector</h2><div class=\"object-view\">";
    detail::appendReflectedObjectHtml(html, "SceneManager", rttr::type::get<Play::SceneManager>(), rttr::instance(*_sceneManager));
    html += "</div></section></section>";
}

} // namespace Play::editor
