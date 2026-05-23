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

void appendMetadataAttribute(std::string& html, const rttr::property& property, const char* metadataName, const char* attributeName)
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

void appendScalarInputAttributes(std::string& html, const rttr::property& property)
{
    appendMetadataAttribute(html, property, "ui.min", "min");
    appendMetadataAttribute(html, property, "ui.max", "max");
    appendMetadataAttribute(html, property, "ui.step", "step");
    if (metadataToString(property, "ui.step").empty())
    {
        html += " step=\"any\"";
    }
}

void appendEditorPropertyAttribute(std::string& html, const std::string& propertyPath)
{
    html += " data-editor-property=\"";
    appendHtmlText(html, propertyPath);
    html += "\"";
}

void appendVectorPropertyInput(std::string& html, const rttr::property& property, const rttr::variant& value, bool editable)
{
    static const char* componentNames[] = {"x", "y", "z", "w"};

    const size_t componentCount = getVectorComponentCount(property.get_type());
    html += "<div class=\"vector-input vector-size-";
    html += std::to_string(componentCount);
    html += "\">";

    for (size_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
    {
        const std::string propertyPath = property.get_name().to_string() + "." + componentNames[componentIndex];
        html += "<input type=\"number\" value=\"";
        appendPropertyValue(html, rttr::variant(getVectorComponent(value, componentIndex)));
        html += "\" title=\"";
        appendHtmlText(html, propertyPath);
        html += "\"";
        appendScalarInputAttributes(html, property);

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

void appendPropertyInput(std::string& html, const rttr::property& property, const rttr::variant& value, bool editable)
{
    const bool propertyEditable = editable && isEditableProperty(property);
    const bool boolProperty     = isBoolProperty(property);
    const bool stringProperty   = isStringProperty(property);
    const bool vectorProperty   = getVectorComponentCount(property.get_type()) > 0;

    if (vectorProperty)
    {
        appendVectorPropertyInput(html, property, value, editable);
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
    }
    else if (stringProperty)
    {
        html += " type=\"text\" value=\"";
        appendPropertyValue(html, value);
        html += "\"";
    }
    else if (property.get_type().is_arithmetic() || property.get_type().is_enumeration())
    {
        const std::string widget = metadataToString(property, "ui.widget");
        html += widget == "slider" ? " type=\"range\"" : " type=\"number\"";
        html += " value=\"";
        appendPropertyValue(html, value);
        html += "\"";
        appendScalarInputAttributes(html, property);
    }
    else
    {
        html += " type=\"text\"";
        html += " value=\"";
        appendPropertyValue(html, value);
        html += "\"";
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

void appendReflectedObjectHtml(std::string& html, const char* title, rttr::type type, rttr::instance instance, bool editable)
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
            appendPropertyInput(html, property, property.get_value(instance), editable);
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
