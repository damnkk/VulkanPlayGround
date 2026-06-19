#include "editor/EditorUiModel.h"

#include <glm/glm.hpp>
#include <rttr/property.h>

namespace Play::editor::detail
{

namespace
{
bool isEditableProperty(const rttr::property& property)
{
    const rttr::type type = property.get_type();
    return !property.is_readonly() &&
           (type.is_arithmetic() || type.is_enumeration() || type == rttr::type::get<std::string>() || type == rttr::type::get<glm::vec2>() ||
            type == rttr::type::get<glm::vec3>() || type == rttr::type::get<glm::vec4>());
}

bool isBoolType(rttr::type type)
{
    return type == rttr::type::get<bool>();
}

bool isStringType(rttr::type type)
{
    return type == rttr::type::get<std::string>();
}

size_t getVectorComponentCount(rttr::type type)
{
    if (type == rttr::type::get<glm::vec2>())
    {
        return 2;
    }

    if (type == rttr::type::get<glm::vec3>())
    {
        return 3;
    }

    if (type == rttr::type::get<glm::vec4>())
    {
        return 4;
    }

    return 0;
}

float getVectorComponent(const rttr::variant& value, size_t componentIndex)
{
    if (value.is_type<glm::vec2>())
    {
        return value.get_value<glm::vec2>()[static_cast<glm::vec2::length_type>(componentIndex)];
    }

    if (value.is_type<glm::vec3>())
    {
        return value.get_value<glm::vec3>()[static_cast<glm::vec3::length_type>(componentIndex)];
    }

    if (value.is_type<glm::vec4>())
    {
        return value.get_value<glm::vec4>()[static_cast<glm::vec4::length_type>(componentIndex)];
    }

    return 0.0f;
}

std::string metadataToString(const rttr::property& property, const char* name)
{
    const rttr::variant metadata = property.get_metadata(name);
    if (!metadata.is_valid())
    {
        return {};
    }

    bool              ok   = false;
    const std::string text = metadata.to_string(&ok);
    return ok ? text : std::string();
}

bool metadataToDouble(const rttr::property& property, const char* name, double& value)
{
    const rttr::variant metadata = property.get_metadata(name);
    if (!metadata.is_valid())
    {
        return false;
    }

    bool         ok   = false;
    const double data = metadata.to_double(&ok);
    if (!ok)
    {
        return false;
    }

    value = data;
    return true;
}

std::string inferFloatingStep(double value)
{
    const double magnitude = value < 0.0 ? -value : value;
    if (magnitude >= 1000.0)
    {
        return "10";
    }

    if (magnitude >= 100.0)
    {
        return "1";
    }

    if (magnitude >= 10.0)
    {
        return "0.1";
    }

    if (magnitude >= 1.0)
    {
        return "0.01";
    }

    if (magnitude >= 0.1)
    {
        return "0.001";
    }

    if (magnitude >= 0.01)
    {
        return "0.0001";
    }

    if (magnitude >= 0.001)
    {
        return "0.00001";
    }

    return "0.000001";
}

std::string inferRangeStep(double minValue, double maxValue)
{
    double range = maxValue - minValue;
    if (range < 0.0)
    {
        range = -range;
    }

    if (range >= 1000.0)
    {
        return "10";
    }

    if (range >= 100.0)
    {
        return "1";
    }

    if (range >= 10.0)
    {
        return "0.1";
    }

    if (range >= 1.0)
    {
        return "0.01";
    }

    if (range >= 0.1)
    {
        return "0.001";
    }

    if (range >= 0.01)
    {
        return "0.0001";
    }

    return "0.00001";
}

bool isFloatingNumericType(rttr::type type)
{
    return type == rttr::type::get<float>() || type == rttr::type::get<double>() || getVectorComponentCount(type) > 0;
}

bool makeTypedPropertyValue(rttr::type targetType, const rttr::variant& value, rttr::variant& typedValue)
{
    if (value.get_type() == targetType)
    {
        typedValue = value;
        return true;
    }

    bool ok = false;
    if (targetType == rttr::type::get<float>())
    {
        const float numericValue = value.to_float(&ok);
        if (ok)
        {
            typedValue = rttr::variant(numericValue);
            return true;
        }
    }
    else if (targetType == rttr::type::get<double>())
    {
        const double numericValue = value.to_double(&ok);
        if (ok)
        {
            typedValue = rttr::variant(numericValue);
            return true;
        }
    }
    else if (targetType == rttr::type::get<int>())
    {
        const double numericValue = value.to_double(&ok);
        if (ok)
        {
            typedValue = rttr::variant(static_cast<int>(numericValue));
            return true;
        }
    }
    else if (targetType == rttr::type::get<unsigned int>())
    {
        const double numericValue = value.to_double(&ok);
        if (ok && numericValue >= 0.0)
        {
            typedValue = rttr::variant(static_cast<unsigned int>(numericValue));
            return true;
        }
    }

    typedValue = value;
    return typedValue.convert(rttr::type(targetType));
}

std::string makeNumericStep(const rttr::property& property, const rttr::variant& value)
{
    const std::string metadataStep = metadataToString(property, "ui.step");
    if (!metadataStep.empty())
    {
        return metadataStep;
    }

    if (!isFloatingNumericType(property.get_type()))
    {
        return "1";
    }

    double minValue = 0.0;
    double maxValue = 0.0;
    if (metadataToDouble(property, "ui.min", minValue) && metadataToDouble(property, "ui.max", maxValue))
    {
        return inferRangeStep(minValue, maxValue);
    }

    bool         ok           = false;
    const double numericValue = value.to_double(&ok);
    return inferFloatingStep(ok ? numericValue : 1.0);
}

std::string makePropertyLabel(const rttr::property& property)
{
    const rttr::variant label = property.get_metadata("ui.label");
    if (label.is_valid())
    {
        bool              ok        = false;
        const std::string labelText = label.to_string(&ok);
        if (ok)
        {
            return labelText;
        }
    }

    return property.get_name().to_string();
}

void applyNumericMetadata(EditorUiProperty& item, const rttr::property& property, const rttr::variant& value)
{
    item.step = makeNumericStep(property, value);

    double minValue = 0.0;
    if (metadataToDouble(property, "ui.min", minValue))
    {
        item.hasMinimum = true;
        item.minimum    = std::to_string(minValue);
    }

    double maxValue = 0.0;
    if (metadataToDouble(property, "ui.max", maxValue))
    {
        item.hasMaximum = true;
        item.maximum    = std::to_string(maxValue);
    }
}

void appendScalarProperty(const rttr::property& property, const rttr::variant& value, const rttr::variant& defaultValue, bool editable,
                          std::vector<EditorUiProperty>& properties)
{
    const rttr::type type = property.get_type();

    EditorUiProperty item;
    item.path         = property.get_name().to_string();
    item.label        = makePropertyLabel(property);
    item.typeName     = type.get_name().to_string();
    item.value        = propertyValueToString(value);
    item.hasDefault   = defaultValue.is_valid();
    item.defaultValue = item.hasDefault ? propertyValueToString(defaultValue) : item.value;
    item.editable     = editable && isEditableProperty(property);

    if (isBoolType(type))
    {
        item.kind      = EditorUiPropertyKind::Bool;
        item.boolValue = value.to_bool();
    }
    else if (type.is_arithmetic())
    {
        item.kind = EditorUiPropertyKind::Number;
        applyNumericMetadata(item, property, value);
    }
    else if (isStringType(type) || type.is_enumeration())
    {
        item.kind = EditorUiPropertyKind::Text;
    }

    properties.push_back(item);
}

void appendVectorProperty(const rttr::property& property, const rttr::variant& value, const rttr::variant& defaultValue, bool editable,
                          std::vector<EditorUiProperty>& properties)
{
    static const char* componentNames[] = {"x", "y", "z", "w"};

    const size_t componentCount = getVectorComponentCount(property.get_type());
    for (size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
    {
        const rttr::variant componentValue        = rttr::variant(getVectorComponent(value, componentIndex));
        const rttr::variant defaultComponentValue = rttr::variant(getVectorComponent(defaultValue, componentIndex));

        EditorUiProperty item;
        item.path         = property.get_name().to_string() + "." + componentNames[componentIndex];
        item.label        = makePropertyLabel(property) + "." + componentNames[componentIndex];
        item.typeName     = property.get_type().get_name().to_string();
        item.value        = propertyValueToString(componentValue);
        item.hasDefault   = true;
        item.defaultValue = propertyValueToString(defaultComponentValue);
        item.kind         = EditorUiPropertyKind::Number;
        item.editable     = editable && isEditableProperty(property);
        applyNumericMetadata(item, property, componentValue);
        properties.push_back(item);
    }
}

int getVectorComponentIndex(const std::string& componentName)
{
    if (componentName == "x" || componentName == "r" || componentName == "0")
    {
        return 0;
    }

    if (componentName == "y" || componentName == "g" || componentName == "1")
    {
        return 1;
    }

    if (componentName == "z" || componentName == "b" || componentName == "2")
    {
        return 2;
    }

    if (componentName == "w" || componentName == "a" || componentName == "3")
    {
        return 3;
    }

    return -1;
}

bool setVectorComponent(rttr::type type, rttr::variant value, int componentIndex, float componentValue, rttr::variant& outputValue)
{
    const size_t componentCount = getVectorComponentCount(type);
    if (componentIndex < 0 || static_cast<size_t>(componentIndex) >= componentCount)
    {
        return false;
    }

    if (type == rttr::type::get<glm::vec2>())
    {
        glm::vec2 vectorValue                                            = value.get_value<glm::vec2>();
        vectorValue[static_cast<glm::vec2::length_type>(componentIndex)] = componentValue;
        outputValue                                                      = rttr::variant(vectorValue);
        return true;
    }

    if (type == rttr::type::get<glm::vec3>())
    {
        glm::vec3 vectorValue                                            = value.get_value<glm::vec3>();
        vectorValue[static_cast<glm::vec3::length_type>(componentIndex)] = componentValue;
        outputValue                                                      = rttr::variant(vectorValue);
        return true;
    }

    if (type == rttr::type::get<glm::vec4>())
    {
        glm::vec4 vectorValue                                            = value.get_value<glm::vec4>();
        vectorValue[static_cast<glm::vec4::length_type>(componentIndex)] = componentValue;
        outputValue                                                      = rttr::variant(vectorValue);
        return true;
    }

    return false;
}
} // namespace

std::string propertyValueToString(const rttr::variant& value)
{
    if (!value.is_valid())
    {
        return {};
    }

    if (value.is_type<bool>())
    {
        return value.to_bool() ? "true" : "false";
    }

    bool              ok   = false;
    const std::string text = value.to_string(&ok);
    return ok ? text : std::string();
}

void appendReflectedObjectProperties(rttr::type type, rttr::instance instance, rttr::instance defaultInstance, bool editable,
                                     std::vector<EditorUiProperty>& properties)
{
    if (!type.is_valid() || !instance.is_valid())
    {
        return;
    }

    for (const rttr::property& property : type.get_properties())
    {
        const rttr::variant value        = property.get_value(instance);
        const rttr::variant defaultValue = defaultInstance.is_valid() ? property.get_value(defaultInstance) : value;
        if (getVectorComponentCount(property.get_type()) > 0)
        {
            appendVectorProperty(property, value, defaultValue, editable, properties);
        }
        else
        {
            appendScalarProperty(property, value, defaultValue, editable, properties);
        }
    }
}

bool setReflectedProperty(rttr::type type, rttr::instance instance, const char* propertyName, const rttr::variant& value)
{
    if (!type.is_valid() || !instance.is_valid())
    {
        return false;
    }

    const std::string propertyPath = propertyName ? propertyName : "";
    const size_t      componentSep = propertyPath.find('.');
    const std::string rootProperty = componentSep == std::string::npos ? propertyPath : propertyPath.substr(0, componentSep);

    const rttr::property property = type.get_property(rootProperty.c_str());
    if (!property || property.is_readonly())
    {
        return false;
    }

    if (componentSep != std::string::npos)
    {
        const int componentIndex = getVectorComponentIndex(propertyPath.substr(componentSep + 1));

        bool          ok             = false;
        const float   componentValue = value.to_float(&ok);
        rttr::variant typedValue;
        if (!ok || !setVectorComponent(property.get_type(), property.get_value(instance), componentIndex, componentValue, typedValue))
        {
            return false;
        }

        return property.set_value(instance, typedValue);
    }

    rttr::variant typedValue;
    if (!makeTypedPropertyValue(property.get_type(), value, typedValue))
    {
        return false;
    }

    return property.set_value(instance, typedValue);
}

} // namespace Play::editor::detail
