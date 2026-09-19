#ifndef PLAY_CODE_EDITOR_INSPECTORWIDGET_H
#define PLAY_CODE_EDITOR_INSPECTORWIDGET_H

#include <QScrollArea>

#include "editor/EditorProtocol.h"

class QLabel;
class QDoubleSpinBox;

namespace Play::editor
{

class RuntimeEditor;

class InspectorWidget final : public QScrollArea
{
public:
    InspectorWidget(RuntimeEditor& editor, const EditorSnapshot& snapshot, QWidget* parent = nullptr);

    void refresh();
    void refreshPendingValues();
    void selectNode(EditorNodeId nodeId);

private:
    void              rebuild();
    const EditorNode* selectedNode() const;
    void              updateValues(const EditorNode& node);

    RuntimeEditor& _editor;
    // The main window owns the current snapshot for the lifetime of its panels.
    const EditorSnapshot&              _snapshot;
    EditorNodeId                       _selectedNode = InvalidEditorNodeId;
    std::vector<EditorComponentId>     _componentIds;
    bool                               _pendingValues       = false;
    QLabel*                            _title               = nullptr;
    QDoubleSpinBox*                    _transformEditors[9] = {};
    std::vector<std::vector<QWidget*>> _propertyWidgets;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_INSPECTORWIDGET_H
