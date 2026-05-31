#ifndef PLAY_CODE_EDITOR_CONTROLPANEL_H
#define PLAY_CODE_EDITOR_CONTROLPANEL_H

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

    void buildSnapshot(EditorUiRenderMode& renderMode) const;

private:
    EditorRegistry&  _registry;
    EditorRenderMode _renderMode = EditorRenderMode::Any;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_CONTROLPANEL_H
