#pragma once

#include "editor/EditorRegistry.h"

namespace Play::runtime
{
class VulkanRuntime;
}

namespace Play::editor
{

void registerRuntimeEditorObjects(EditorRegistry& registry, Play::runtime::VulkanRuntime& runtime);

} // namespace Play::editor
