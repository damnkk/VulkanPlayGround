#include "editor/AssetBrowserWidget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "editor/RuntimeEditor.h"

namespace Play::editor
{

AssetBrowserWidget::AssetBrowserWidget(RuntimeEditor& editor, QWidget* parent) : QWidget(parent), _editor(editor)
{
    QLabel* typeLabel = new QLabel(tr("Type"), this);
    _typeSelector     = new QComboBox(this);
    _typeSelector->addItem(tr("Model"), static_cast<int>(EditorAssetType::Model));
    _typeSelector->setMinimumWidth(120);

    QPushButton* importButton = new QPushButton(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("Import"), this);
    importButton->setToolTip(tr("Import external resources into the project"));
    connect(importButton, &QPushButton::clicked, this, &AssetBrowserWidget::importSelectedAssetType);

    QHBoxLayout* actions = new QHBoxLayout();
    actions->setContentsMargins(6, 4, 6, 4);
    actions->setSpacing(6);
    actions->addWidget(typeLabel);
    actions->addWidget(_typeSelector);
    actions->addStretch();
    actions->addWidget(importButton);

    _tree = new QTreeWidget(this);
    _tree->setHeaderLabels({tr("Name"), tr("Type"), tr("Resource")});
    _tree->setRootIsDecorated(false);
    _tree->setAlternatingRowColors(true);
    _tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    _tree->setSortingEnabled(true);
    _tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    _tree->sortItems(0, Qt::AscendingOrder);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(actions);
    layout->addWidget(_tree);
}

void AssetBrowserWidget::refresh(const EditorSnapshot& snapshot)
{
    bool unchanged = _items.size() == snapshot.assets.size();
    for (const auto& asset : snapshot.assets)
    {
        const auto* item = _items.value(QString::fromStdString(asset.id));
        if (!item || item->text(0) != QString::fromStdString(asset.name) || item->text(1) != QString::fromStdString(asset.type) ||
            item->data(2, Qt::UserRole).toString() != QString::fromStdString(asset.path))
        {
            unchanged = false;
            break;
        }
    }
    if (unchanged) return;

    const QString    selectedId        = _tree->currentItem() ? _tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
    QTreeWidgetItem* restoredSelection = nullptr;
    _tree->clear();
    _items.clear();
    for (const EditorAsset& asset : snapshot.assets)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(_tree);
        item->setText(0, QString::fromStdString(asset.name));
        item->setText(1, QString::fromStdString(asset.type));
        const QString resourcePath = QString::fromStdString(asset.path);
        item->setData(2, Qt::UserRole, resourcePath);
        item->setText(2, resourcePath.isEmpty() ? tr("—") : QDir::toNativeSeparators(resourcePath));
        item->setData(0, Qt::UserRole, QString::fromStdString(asset.id));
        _items.insert(QString::fromStdString(asset.id), item);
        if (!resourcePath.isEmpty()) item->setToolTip(2, QDir::toNativeSeparators(resourcePath));
        if (item->data(0, Qt::UserRole).toString() == selectedId) restoredSelection = item;
    }
    if (restoredSelection) _tree->setCurrentItem(restoredSelection);
}

void AssetBrowserWidget::importSelectedAssetType()
{
    const EditorAssetType type = static_cast<EditorAssetType>(_typeSelector->currentData().toInt());
    QString               caption;
    QString               filter;
    switch (type)
    {
        case EditorAssetType::Model:
            caption = tr("Import Models");
            filter  = tr("3D Models (*.vpgmodel *.glb *.gltf *.fbx *.obj *.dae *.3ds *.ply *.stl);;All Files (*.*)");
            break;
    }

    const QStringList paths = QFileDialog::getOpenFileNames(this, caption, {}, filter);
    for (const QString& path : paths)
    {
        EditorAssetImportRequest request;
        request.type       = type;
        request.sourcePath = path.toUtf8().toStdString();
        _editor.requestAssetImport(std::move(request));
    }
}

} // namespace Play::editor
