#ifndef PLAY_CODE_EDITOR_INSPECTORWIDGET_H
#define PLAY_CODE_EDITOR_INSPECTORWIDGET_H

#include <QScrollArea>

#include "editor/EditorProtocol.h"

namespace Play::editor
{

class RuntimeEditor;

class InspectorWidget final : public QScrollArea
{
public:
    explicit InspectorWidget(RuntimeEditor& editor, QWidget* parent = nullptr);

    void setSnapshot(const EditorSnapshot& snapshot);
    void selectNode(EditorNodeId nodeId);

private:
    void rebuild();

    RuntimeEditor& _editor;
    EditorSnapshot _snapshot;
    EditorNodeId   _selectedNode = InvalidEditorNodeId;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_INSPECTORWIDGET_H
