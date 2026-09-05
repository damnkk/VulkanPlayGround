#ifndef PLAY_CODE_EDITOR_EDITORREFLECTION_H
#define PLAY_CODE_EDITOR_EDITORREFLECTION_H

#include "editor/EditorProtocol.h"

#include <rttr/instance.h>

namespace Play::editor
{

void reflectProperties(rttr::type type, rttr::instance instance, std::vector<EditorProperty>& output);

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_EDITORREFLECTION_H
