#include "editor/EditorHtml.h"

#include <glm/glm.hpp>
#include <rttr/property.h>

namespace Play::editor::detail
{

namespace
{
void appendPropertyValue(std::string& html, const rttr::variant& value)
{
    if (!value.is_valid())
    {
        return;
    }

    if (value.is_type<bool>())
    {
        appendHtmlText(html, value.to_bool() ? "true" : "false");
        return;
    }

    bool              ok   = false;
    const std::string text = value.to_string(&ok);
    if (ok)
    {
        appendHtmlText(html, text);
    }
}

void appendTypeName(std::string& html, rttr::type type)
{
    if (!type.is_valid())
    {
        appendHtmlText(html, "Unreflected Type");
        return;
    }

    appendHtmlText(html, type.get_name().to_string());
}

bool isEditableProperty(const rttr::property& property)
{
    const rttr::type type = property.get_type();
    return !property.is_readonly()
           && (type.is_arithmetic() || type.is_enumeration() || type == rttr::type::get<std::string>() || type == rttr::type::get<glm::vec2>()
               || type == rttr::type::get<glm::vec3>() || type == rttr::type::get<glm::vec4>());
}

bool isBoolProperty(const rttr::property& property)
{
    return property.get_type() == rttr::type::get<bool>();
}

bool isStringProperty(const rttr::property& property)
{
    return property.get_type() == rttr::type::get<std::string>();
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

bool isFloatingNumericProperty(const rttr::property& property)
{
    const rttr::type type = property.get_type();
    return type == rttr::type::get<float>() || type == rttr::type::get<double>() || getVectorComponentCount(type) > 0;
}

std::string makeNumericStep(const rttr::property& property, const rttr::variant& value)
{
    const std::string metadataStep = metadataToString(property, "ui.step");
    if (!metadataStep.empty())
    {
        return metadataStep;
    }

    if (!isFloatingNumericProperty(property))
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

void appendMetadataDataAttribute(std::string& html, const rttr::property& property, const char* metadataName, const char* attributeName)
{
    const std::string value = metadataToString(property, metadataName);
    if (value.empty())
    {
        return;
    }

    html += " ";
    html += attributeName;
    html += "=\"";
    appendHtmlText(html, value);
    html += "\"";
}

void appendNumericInputAttributes(std::string& html, const rttr::property& property, const rttr::variant& value)
{
    html += " inputmode=\"decimal\" data-editor-drag=\"number\" data-editor-step=\"";
    appendHtmlText(html, makeNumericStep(property, value));
    html += "\"";
    appendMetadataDataAttribute(html, property, "ui.min", "data-editor-min");
    appendMetadataDataAttribute(html, property, "ui.max", "data-editor-max");
}

void appendEditorPropertyAttribute(std::string& html, const std::string& propertyPath)
{
    html += " data-editor-property=\"";
    appendHtmlText(html, propertyPath);
    html += "\"";
}

void appendInputDefaultTextAttribute(std::string& html, const char* value)
{
    html += " data-editor-default=\"";
    appendHtmlText(html, value);
    html += "\"";
}

void appendInputDefaultAttribute(std::string& html, const rttr::variant& value)
{
    html += " data-editor-default=\"";
    appendPropertyValue(html, value);
    html += "\"";
}

void appendVectorPropertyInput(
    std::string& html, const rttr::property& property, const rttr::variant& value, const rttr::variant& defaultValue, bool editable)
{
    static const char* componentNames[] = {"x", "y", "z", "w"};

    const size_t componentCount = getVectorComponentCount(property.get_type());
    html += "<div class=\"vector-input vector-size-";
    html += std::to_string(componentCount);
    html += "\">";

    for (size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
    {
        const std::string propertyPath = property.get_name().to_string() + "." + componentNames[componentIndex];
        const rttr::variant componentValue = rttr::variant(getVectorComponent(value, componentIndex));
        const rttr::variant defaultComponentValue = rttr::variant(getVectorComponent(defaultValue, componentIndex));
        html += "<input type=\"text\" value=\"";
        appendPropertyValue(html, componentValue);
        html += "\" title=\"";
        appendHtmlText(html, propertyPath);
        html += "\"";
        appendNumericInputAttributes(html, property, componentValue);
        appendInputDefaultAttribute(html, defaultComponentValue);

        if (editable && !property.is_readonly())
        {
            appendEditorPropertyAttribute(html, propertyPath);
        }
        else
        {
            html += " readonly";
        }

        html += ">";
    }

    html += "</div>";
}

void appendPropertyInput(
    std::string& html, const rttr::property& property, const rttr::variant& value, const rttr::variant& defaultValue, bool editable)
{
    const bool propertyEditable = editable && isEditableProperty(property);
    const bool boolProperty     = isBoolProperty(property);
    const bool stringProperty   = isStringProperty(property);
    const bool vectorProperty   = getVectorComponentCount(property.get_type()) > 0;

    if (vectorProperty)
    {
        appendVectorPropertyInput(html, property, value, defaultValue, editable);
        return;
    }

    html += "<input";
    if (boolProperty)
    {
        html += " type=\"checkbox\"";
        if (value.to_bool())
        {
            html += " checked";
        }
        appendInputDefaultTextAttribute(html, defaultValue.to_bool() ? "true" : "false");
    }
    else if (stringProperty)
    {
        html += " type=\"text\" value=\"";
        appendPropertyValue(html, value);
        html += "\"";
        appendInputDefaultAttribute(html, defaultValue);
    }
    else if (property.get_type().is_arithmetic() || property.get_type().is_enumeration())
    {
        html += " type=\"text\"";
        html += " value=\"";
        appendPropertyValue(html, value);
        html += "\"";
        appendNumericInputAttributes(html, property, value);
        appendInputDefaultAttribute(html, defaultValue);
    }
    else
    {
        html += " type=\"text\"";
        html += " value=\"";
        appendPropertyValue(html, value);
        html += "\"";
        appendInputDefaultAttribute(html, defaultValue);
    }

    if (propertyEditable)
    {
        appendEditorPropertyAttribute(html, property.get_name().to_string());
    }
    else
    {
        html += " readonly";
        if (boolProperty)
        {
            html += " disabled";
        }
    }

    html += ">";
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
        glm::vec2 vectorValue = value.get_value<glm::vec2>();
        vectorValue[static_cast<glm::vec2::length_type>(componentIndex)] = componentValue;
        outputValue = rttr::variant(vectorValue);
        return true;
    }

    if (type == rttr::type::get<glm::vec3>())
    {
        glm::vec3 vectorValue = value.get_value<glm::vec3>();
        vectorValue[static_cast<glm::vec3::length_type>(componentIndex)] = componentValue;
        outputValue = rttr::variant(vectorValue);
        return true;
    }

    if (type == rttr::type::get<glm::vec4>())
    {
        glm::vec4 vectorValue = value.get_value<glm::vec4>();
        vectorValue[static_cast<glm::vec4::length_type>(componentIndex)] = componentValue;
        outputValue = rttr::variant(vectorValue);
        return true;
    }

    return false;
}
} // namespace

void appendHtmlText(std::string& html, const char* text)
{
    if (!text)
    {
        return;
    }

    for (const char* cursor = text; *cursor; ++cursor)
    {
        const char ch = *cursor;
        switch (ch)
        {
            case '&':
                html += "&amp;";
                break;
            case '<':
                html += "&lt;";
                break;
            case '>':
                html += "&gt;";
                break;
            case '"':
                html += "&quot;";
                break;
            case '\'':
                html += "&#39;";
                break;
            default:
                html += ch;
                break;
        }
    }
}

void appendHtmlText(std::string& html, const std::string& text)
{
    appendHtmlText(html, text.c_str());
}

void appendReflectedObjectHtml(
    std::string& html, const char* title, rttr::type type, rttr::instance instance, bool editable, rttr::instance defaultInstance)
{
    html += "<div class=\"object-head\"><div class=\"object-title\">";
    appendHtmlText(html, title);
    html += "</div><div class=\"object-type\">";
    appendTypeName(html, type);
    html += "</div></div>";

    size_t propertyCount = 0;
    html += "<div class=\"property-list\">";
    if (type.is_valid() && instance.is_valid())
    {
        for (const rttr::property& property : type.get_properties())
        {
            ++propertyCount;
            html += "<div class=\"property-row\"><label title=\"";
            appendHtmlText(html, property.get_name().to_string());
            html += " : ";
            appendHtmlText(html, property.get_type().get_name().to_string());
            html += "\">";

            const rttr::variant label = property.get_metadata("ui.label");
            if (label.is_valid())
            {
                bool              ok        = false;
                const std::string labelText = label.to_string(&ok);
                appendHtmlText(html, ok ? labelText : property.get_name().to_string());
            }
            else
            {
                appendHtmlText(html, property.get_name().to_string());
            }

            html += "</label>";
            const rttr::variant value        = property.get_value(instance);
            const rttr::variant defaultValue = defaultInstance.is_valid() ? property.get_value(defaultInstance) : value;
            appendPropertyInput(html, property, value, defaultValue, editable);
            html += "</div>";
        }
    }
    html += "</div>";

    if (propertyCount == 0)
    {
        html += "<div class=\"empty\">No reflected properties.</div>";
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

        bool        ok             = false;
        const float componentValue = value.to_float(&ok);
        rttr::variant typedValue;
        if (!ok || !setVectorComponent(property.get_type(), property.get_value(instance), componentIndex, componentValue, typedValue))
        {
            return false;
        }

        return property.set_value(instance, typedValue);
    }

    rttr::variant typedValue = value;
    if (typedValue.get_type() != property.get_type() && !typedValue.convert(property.get_type()))
    {
        return false;
    }

    return property.set_value(instance, typedValue);
}

} // namespace Play::editor::detail
