#include "editor/EditorReflection.h"

#include <rttr/property.h>

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
        item.label    = item.name;
        item.type     = property.get_type();
        item.value    = property.get_value(instance);
        item.readOnly = property.is_readonly();

        const rttr::variant label = property.get_metadata("ui.label");
        if (label.is_valid())
        {
            item.label = label.to_string();
        }

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
        }

        output.push_back(std::move(item));
    }
}

} // namespace Play::editor
