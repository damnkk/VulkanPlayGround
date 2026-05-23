#include "editor/EditorHtml.h"

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

void appendReflectedObjectHtml(std::string& html, const char* title, rttr::type type, rttr::instance instance)
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

            html += "</label><input readonly value=\"";
            appendPropertyValue(html, property.get_value(instance));
            html += "\"></div>";
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

    const rttr::property property = type.get_property(propertyName ? propertyName : "");
    if (!property || property.is_readonly())
    {
        return false;
    }

    rttr::variant typedValue = value;
    if (typedValue.get_type() != property.get_type() && !typedValue.convert(property.get_type()))
    {
        return false;
    }

    return property.set_value(instance, typedValue);
}

} // namespace Play::editor::detail
