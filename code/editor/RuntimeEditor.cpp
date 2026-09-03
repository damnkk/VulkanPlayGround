#include "editor/RuntimeEditor.h"

namespace Play::editor
{

RuntimeEditor::RuntimeEditor() = default;

void RuntimeEditor::bindRuntime(Play::runtime::VulkanRuntime&, Play::RenderSession&, const char*) {}

void RuntimeEditor::setStartupProjectReceiver(StartupProjectReceiver* receiver)
{
    _startupProjectReceiver = receiver;
}

} // namespace Play::editor
