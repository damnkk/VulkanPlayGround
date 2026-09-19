#ifndef PLAY_CODE_EDITOR_ASSETBROWSERWIDGET_H
#define PLAY_CODE_EDITOR_ASSETBROWSERWIDGET_H

#include <QWidget>
#include <QHash>

#include "editor/EditorProtocol.h"

class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;

namespace Play::editor
{

class RuntimeEditor;

class AssetBrowserWidget final : public QWidget
{
public:
    explicit AssetBrowserWidget(RuntimeEditor& editor, QWidget* parent = nullptr);

    void refresh(const EditorSnapshot& snapshot);
    void importSelectedAssetType();

private:
    RuntimeEditor&                   _editor;
    QComboBox*                       _typeSelector = nullptr;
    QTreeWidget*                     _tree         = nullptr;
    QHash<QString, QTreeWidgetItem*> _items;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_ASSETBROWSERWIDGET_H
