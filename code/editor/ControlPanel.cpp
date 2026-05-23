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

        const rttr::instance defaultInstance = _registry.getDefaultObjectInstance(id);

        html += "<details class=\"control-unit\" data-editor-object-id=\"";
        html += std::to_string(id);
        html += "\" open><summary><span>";
        detail::appendHtmlText(html, objectInfo->title);
        html += "</span>";
        if (defaultInstance.is_valid())
        {
            html += "<button class=\"control-reset\" type=\"button\" title=\"Reset to default\" data-editor-reset-object>";
            html += "&#8634;</button>";
        }
        html += "</summary>";
        html += "<div class=\"control-content\">";
        detail::appendReflectedObjectHtml(
            html, objectInfo->title.c_str(), objectInfo->type, _registry.getObjectInstance(id), true, defaultInstance);
        html += "</div></details>";
    }
    html += "</div></section>";
}

} // namespace Play::editor
