#ifndef PLAY_CODE_EDITOR_ASSETBROWSERWIDGET_H
#define PLAY_CODE_EDITOR_ASSETBROWSERWIDGET_H

#include <QWidget>

#include "editor/EditorProtocol.h"

class QTreeWidget;

namespace Play::editor
{

class AssetBrowserWidget final : public QWidget
{
public:
    explicit AssetBrowserWidget(QWidget* parent = nullptr);

    void refresh(const EditorSnapshot& snapshot);

private:
    QTreeWidget* _tree = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_ASSETBROWSERWIDGET_H
