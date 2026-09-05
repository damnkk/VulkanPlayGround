#include "editor/SceneTreeWidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include "editor/RuntimeEditor.h"

namespace Play::editor
{

SceneTreeWidget::SceneTreeWidget(RuntimeEditor& editor, QWidget* parent) : QWidget(parent), _editor(editor)
{
    QPushButton* addRoot  = new QPushButton("Add Root", this);
    QPushButton* addChild = new QPushButton("Add Child", this);
    QPushButton* remove   = new QPushButton("Remove", this);
    addChild->setEnabled(false);
    remove->setEnabled(false);

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->addWidget(addRoot);
    toolbar->addWidget(addChild);
    toolbar->addWidget(remove);

    _tree = new QTreeWidget(this);
    _tree->setHeaderHidden(true);
    _tree->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(toolbar);
    layout->addWidget(_tree);

    const auto addNode = [this](bool asChild)
    {
        EditorCommand command;
        command.type     = EditorCommandType::CreateNode;
        command.parentId = asChild && _tree->currentItem() ? _tree->currentItem()->data(0, Qt::UserRole).toULongLong() : InvalidEditorNodeId;
        if (asChild && _tree->currentItem())
        {
            _tree->currentItem()->setExpanded(true);
        }
        _editor.submit(std::move(command));
    };

    connect(addRoot, &QPushButton::clicked, this, [addNode]() { addNode(false); });
    connect(addChild, &QPushButton::clicked, this, [addNode]() { addNode(true); });
    connect(remove, &QPushButton::clicked, this,
            [this]()
            {
                if (!_tree->currentItem())
                {
                    return;
                }
                EditorCommand command;
                command.type   = EditorCommandType::RemoveNode;
                command.nodeId = _tree->currentItem()->data(0, Qt::UserRole).toULongLong();
                _editor.submit(std::move(command));
            });
    connect(_tree, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current) { emit nodeSelected(current ? current->data(0, Qt::UserRole).toULongLong() : InvalidEditorNodeId); });
    connect(this, &SceneTreeWidget::nodeSelected, this,
            [addChild, remove](qulonglong nodeId)
            {
                addChild->setEnabled(nodeId != InvalidEditorNodeId);
                remove->setEnabled(nodeId != InvalidEditorNodeId);
            });
    connect(_tree, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem* item)
            {
                EditorCommand command;
                command.type   = EditorCommandType::RenameNode;
                command.nodeId = item->data(0, Qt::UserRole).toULongLong();
                command.name   = item->text(0).toStdString();
                _editor.submit(std::move(command));
            });
}

void SceneTreeWidget::refresh(const EditorSnapshot& snapshot)
{
    QTreeWidgetItem*        selected   = _tree->currentItem();
    const EditorNodeId      selectedId = selected ? selected->data(0, Qt::UserRole).toULongLong() : InvalidEditorNodeId;
    const EditorNodeId      parentId = selected && selected->parent() ? selected->parent()->data(0, Qt::UserRole).toULongLong() : InvalidEditorNodeId;
    QSet<EditorNodeId>      expanded;
    QTreeWidgetItemIterator iterator(_tree);
    while (*iterator)
    {
        if ((*iterator)->isExpanded())
        {
            expanded.insert((*iterator)->data(0, Qt::UserRole).toULongLong());
        }
        ++iterator;
    }

    const QSignalBlocker blocker(_tree);
    _tree->clear();
    QHash<EditorNodeId, QTreeWidgetItem*> items;
    for (const EditorNode& node : snapshot.nodes)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, QString::fromStdString(node.name));
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(node.id));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        items.insert(node.id, item);
    }
    for (const EditorNode& node : snapshot.nodes)
    {
        QTreeWidgetItem* item   = items.value(node.id);
        QTreeWidgetItem* parent = items.value(node.parentId);
        if (parent)
        {
            parent->addChild(item);
        }
        else
        {
            _tree->addTopLevelItem(item);
        }
        item->setExpanded(expanded.contains(node.id));
    }

    // Restore selection only after the whole tree is attached. When the selected
    // node was deleted, continue editing its parent (or a remaining root).
    QTreeWidgetItem* current = items.value(selectedId);
    if (!current)
    {
        current = items.value(parentId);
    }
    if (!current && _tree->topLevelItemCount() > 0)
    {
        current = _tree->topLevelItem(0);
    }
    _tree->setCurrentItem(current);
    // Tree signals were blocked during rebuilding; publish the actual final
    // selection so the toolbar and Inspector agree with the visible tree.
    emit nodeSelected(current ? current->data(0, Qt::UserRole).toULongLong() : InvalidEditorNodeId);
}

} // namespace Play::editor
