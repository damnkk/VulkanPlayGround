#ifndef PLAY_CODE_EDITOR_PROPERTYWIDGETFACTORY_H
#define PLAY_CODE_EDITOR_PROPERTYWIDGETFACTORY_H

#include <QWidget>

#include "editor/EditorProtocol.h"

namespace Play::editor
{

class RuntimeEditor;

class PropertyWidgetFactory
{
public:
    static QWidget* create(const EditorProperty& property, const EditorSnapshot& snapshot, EditorNodeId nodeId, EditorComponentId componentId,
                           RuntimeEditor& editor, QWidget* parent);
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_PROPERTYWIDGETFACTORY_H
