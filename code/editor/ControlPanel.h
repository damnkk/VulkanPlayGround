#pragma once

#include "editor/EditorRegistry.h"

namespace Play::editor
{

class ControlPanel
{
public:
    ControlPanel(EditorRegistry& registry, EditorRenderMode renderMode);

    ControlPanel(const ControlPanel&)            = delete;
    ControlPanel& operator=(const ControlPanel&) = delete;
    ControlPanel(ControlPanel&&)                 = delete;
    ControlPanel& operator=(ControlPanel&&)      = delete;

    void appendHtml(std::string& html) const;

private:
    EditorRegistry&   _registry;
    EditorRenderMode  _renderMode = EditorRenderMode::Any;
};

} // namespace Play::editor
