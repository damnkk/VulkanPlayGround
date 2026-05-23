#include "editor/RenderModeEditor.h"

#include "editor/EditorHtml.h"

namespace Play::editor
{

RenderModeEditor::RenderModeEditor(const char* id, const char* title, EditorRegistry& editorRegistry, EditorRenderMode renderMode)
    : _id(id ? id : ""), _title(title ? title : ""), _controlPanel(editorRegistry, renderMode)
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

void RenderModeEditor::appendTabHtml(std::string& html, bool active) const
{
    html += "<button class=\"render-mode-tab";
    html += active ? " active" : "";
    html += "\" data-render-mode=\"";
    detail::appendHtmlText(html, _id);
    html += "\">";
    detail::appendHtmlText(html, _title);
    html += "</button>";
}

void RenderModeEditor::appendPageHtml(std::string& html, bool active) const
{
    html += "<section class=\"render-mode-page";
    html += active ? " active" : "";
    html += "\" data-render-mode-page=\"";
    detail::appendHtmlText(html, _id);
    html += "\">";
    _sceneManagerEditor.appendHtml(html);
    _controlPanel.appendHtml(html);
    html += "</section>";
}

} // namespace Play::editor
