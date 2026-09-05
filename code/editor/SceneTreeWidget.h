#ifndef PLAY_CODE_EDITOR_SCENETREEWIDGET_H
#define PLAY_CODE_EDITOR_SCENETREEWIDGET_H

#include <QWidget>

#include "editor/EditorProtocol.h"

class QTreeWidget;

namespace Play::editor
{

class RuntimeEditor;

class SceneTreeWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SceneTreeWidget(RuntimeEditor& editor, QWidget* parent = nullptr);

    void refresh(const EditorSnapshot& snapshot);

signals:
    void nodeSelected(qulonglong nodeId);

private:
    RuntimeEditor& _editor;
    QTreeWidget*   _tree       = nullptr;
    bool           _refreshing = false;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_SCENETREEWIDGET_H
