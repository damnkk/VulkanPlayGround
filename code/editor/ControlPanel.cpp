#include "editor/ControlPanel.h"

namespace Play::editor
{

ControlPanel::ControlPanel(EditorRegistry& registry, EditorRenderMode renderMode) : _registry(registry), _renderMode(renderMode) {}

void ControlPanel::buildSnapshot(EditorUiRenderMode& renderMode) const
{
    EditorObjectQuery query;
    query.renderMode             = _renderMode;
    query.requiredCapabilityMask = toEditorObjectCapabilityMask(EditorObjectCapability::Editable);

    const std::vector<EditorObjectId> objects = _registry.queryObjects(query);
    for (EditorObjectId id : objects)
    {
        const EditorObjectInfo* objectInfo = _registry.getObjectInfo(id);
        if (!objectInfo)
        {
            continue;
        }

        const rttr::instance defaultInstance = _registry.getDefaultObjectInstance(id);

        EditorUiObject object;
        object.id       = id;
        object.title    = objectInfo->title;
        object.typeName = objectInfo->type.is_valid() ? objectInfo->type.get_name().to_string() : std::string("Unreflected Type");
        object.canReset = defaultInstance.is_valid();

        detail::appendReflectedObjectProperties(objectInfo->type, _registry.getObjectInstance(id), defaultInstance, true, object.properties);

        renderMode.controls.push_back(object);
    }
}

} // namespace Play::editor
