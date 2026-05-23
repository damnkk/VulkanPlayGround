#include "editor/ControlPanel.h"

namespace Play::editor
{

ControlPanel::ControlPanel(EditorRegistry& registry, EditorRenderMode renderMode) : _registry(registry), _renderMode(renderMode)
{
}

void ControlPanel::appendHtml(std::string& html) const
{
    html += "<section class=\"control-panel\"><h2>Control Panel</h2><div class=\"control-grid\">";
    EditorObjectQuery query;
    query.renderMode              = _renderMode;
    query.requiredCapabilityMask  = toEditorObjectCapabilityMask(EditorObjectCapability::Editable);
    const std::vector<EditorObjectId> objects = _registry.queryObjects(query);
    if (objects.empty())
    {
        html += "<div class=\"empty\">No control units.</div>";
    }

    for (EditorObjectId id : objects)
    {
        const EditorObjectInfo* objectInfo = _registry.getObjectInfo(id);
        if (!objectInfo)
        {
            continue;
        }

        html += "<details class=\"control-unit\" data-editor-object-id=\"";
        html += std::to_string(id);
        html += "\" open><summary>";
        detail::appendHtmlText(html, objectInfo->title);
        html += "</summary><div class=\"control-content\">";
        detail::appendReflectedObjectHtml(html, objectInfo->title.c_str(), objectInfo->type, _registry.getObjectInstance(id));
        html += "</div></details>";
    }
    html += "</div></section>";
}

} // namespace Play::editor
