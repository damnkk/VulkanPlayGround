#include "editor/AssetBrowserWidget.h"

#include <QTreeWidget>
#include <QVBoxLayout>

namespace Play::editor
{

AssetBrowserWidget::AssetBrowserWidget(QWidget* parent) : QWidget(parent)
{
    _tree = new QTreeWidget(this);
    _tree->setHeaderLabels({"Name", "Type", "Path"});
    _tree->setRootIsDecorated(false);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(_tree);
}

void AssetBrowserWidget::refresh(const EditorSnapshot& snapshot)
{
    _tree->clear();
    for (const EditorAsset& asset : snapshot.assets)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(_tree);
        item->setText(0, QString::fromStdString(asset.name));
        item->setText(1, QString::fromStdString(asset.type));
        item->setText(2, QString::fromStdString(asset.path));
        item->setData(0, Qt::UserRole, QString::fromStdString(asset.id));
    }
    for (int column = 0; column < 3; ++column)
    {
        _tree->resizeColumnToContents(column);
    }
}

} // namespace Play::editor
