#include "editor/EditorRegistration.h"

#include "controlComponent/controlComponent.h"
#include "core/runtime/VulkanRuntime.h"

namespace Play::editor
{

void registerRuntimeEditorObjects(EditorRegistry& registry, Play::runtime::VulkanRuntime& runtime)
{
    registry.registerWritable<shaderio::TonemapperData>("Tonemapper", runtime.getTonemapperControlComponent(), EditorRenderMode::Defer);
}

} // namespace Play::editor
