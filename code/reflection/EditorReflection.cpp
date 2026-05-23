#include <rttr/registration>

#include "editor/ControlPanel.h"
#include "editor/EditorRegistry.h"
#include "editor/EditorRuntimeContext.h"
#include "editor/MainBar.h"
#include "editor/RenderModeEditor.h"
#include "editor/RenderModeTabs.h"
#include "editor/RuntimeEditor.h"
#include "editor/SceneManagerEditor.h"

RTTR_REGISTRATION
{
    rttr::registration::enumeration<Play::editor::EditorRenderMode>("Play::editor::EditorRenderMode")(
        rttr::value("Any", Play::editor::EditorRenderMode::Any),
        rttr::value("Defer", Play::editor::EditorRenderMode::Defer),
        rttr::value("Gaussian", Play::editor::EditorRenderMode::Gaussian),
        rttr::value("Raytrace", Play::editor::EditorRenderMode::Raytrace));

    rttr::registration::enumeration<Play::editor::EditorObjectCapability>("Play::editor::EditorObjectCapability")(
        rttr::value("None", Play::editor::EditorObjectCapability::None),
        rttr::value("Editable", Play::editor::EditorObjectCapability::Editable),
        rttr::value("Inspectable", Play::editor::EditorObjectCapability::Inspectable),
        rttr::value("Profiled", Play::editor::EditorObjectCapability::Profiled));

    rttr::registration::class_<Play::editor::EditorObjectTraits>("Play::editor::EditorObjectTraits")
        .property("renderMode", &Play::editor::EditorObjectTraits::renderMode)
        .property("capabilityMask", &Play::editor::EditorObjectTraits::capabilityMask);

    rttr::registration::class_<Play::editor::EditorObjectQuery>("Play::editor::EditorObjectQuery")
        .property("renderMode", &Play::editor::EditorObjectQuery::renderMode)
        .property("requiredCapabilityMask", &Play::editor::EditorObjectQuery::requiredCapabilityMask);

    rttr::registration::class_<Play::editor::EditorObjectInfo>("Play::editor::EditorObjectInfo");
    rttr::registration::class_<Play::editor::EditorRegistry>("Play::editor::EditorRegistry");

    rttr::registration::class_<Play::editor::EditorRuntimeContext>("Play::editor::EditorRuntimeContext");
    rttr::registration::class_<Play::editor::MainBar>("Play::editor::MainBar");
    rttr::registration::class_<Play::editor::RenderModeTabs>("Play::editor::RenderModeTabs");
    rttr::registration::class_<Play::editor::RenderModeEditor>("Play::editor::RenderModeEditor");
    rttr::registration::class_<Play::editor::SceneManagerEditor>("Play::editor::SceneManagerEditor");
    rttr::registration::class_<Play::editor::ControlPanel>("Play::editor::ControlPanel");
    rttr::registration::class_<Play::editor::RuntimeEditor>("Play::editor::RuntimeEditor");
}
