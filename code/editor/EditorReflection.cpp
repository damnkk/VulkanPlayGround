#include "editor/EditorReflection.h"

#include <rttr/property.h>
#include <rttr/method.h>
#include "core/assets/Asset.h"

namespace Play::editor
{

void reflectProperties(rttr::type type, rttr::instance instance, std::vector<EditorProperty>& output)
{
    for (const rttr::property& property : type.get_properties())
    {
        const rttr::variant hidden = property.get_metadata("ui.hidden");
        if (hidden.is_valid() && hidden.to_bool())
        {
            continue;
        }

        EditorProperty item;
        item.name     = property.get_name().to_string();
        item.type     = property.get_type();
        item.value    = property.get_value(instance);
        item.readOnly = property.is_readonly();

        const rttr::variant label = property.get_metadata("ui.label");
        item.label                = label.to_string();
        if (item.label.empty()) item.label = item.name;

        const rttr::variant readOnly = property.get_metadata("ui.read_only");
        if (readOnly.is_valid())
        {
            item.readOnly = item.readOnly || readOnly.to_bool();
        }

        const rttr::variant minimum = property.get_metadata("ui.min");
        if (minimum.is_valid())
        {
            item.hasMinimum = true;
            item.minimum    = minimum.to_double();
        }

        const rttr::variant maximum = property.get_metadata("ui.max");
        if (maximum.is_valid())
        {
            item.hasMaximum = true;
            item.maximum    = maximum.to_double();
        }

        const rttr::variant step = property.get_metadata("ui.step");
        if (step.is_valid())
        {
            item.step = step.to_double();
        }

        const rttr::variant resourceType = property.get_metadata("ui.resource_type");
        if (resourceType.is_valid())
        {
            item.resourceType = resourceType.to_string();
            AssetRef asset;
            if (item.value.convert(asset) && asset)
            {
                item.resourceId = uuids::to_string(asset->getUID());
            }
            // Only the resource ID crosses into Qt; the engine retains ownership.
            item.value.clear();
        }

        output.push_back(std::move(item));
    }
}

void reflectActions(rttr::type type, std::vector<EditorAction>& output)
{
    for (const auto& method : type.get_methods())
    {
        if (!method.get_metadata("ui.action").to_bool() || !method.get_parameter_infos().empty()) continue;
        EditorAction action;
        action.name      = method.get_name().to_string();
        const auto label = method.get_metadata("ui.label");
        action.label     = label.to_string();
        if (action.label.empty()) action.label = action.name;
        output.push_back(std::move(action));
    }
}

} // namespace Play::editor
