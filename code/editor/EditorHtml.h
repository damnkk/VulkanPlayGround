#ifndef PLAY_CODE_EDITOR_EDITORHTML_H
#define PLAY_CODE_EDITOR_EDITORHTML_H


#include <rttr/type.h>
#include <rttr/instance.h>
#include <rttr/variant.h>

namespace Play::editor::detail
{

void appendHtmlText(std::string& html, const char* text);
void appendHtmlText(std::string& html, const std::string& text);
void appendReflectedObjectHtml(std::string& html, const char* title, rttr::type type, rttr::instance instance, bool editable = false);
bool setReflectedProperty(rttr::type type, rttr::instance instance, const char* propertyName, const rttr::variant& value);

} // namespace Play::editor::detail

#endif // PLAY_CODE_EDITOR_EDITORHTML_H
