#include "editor/QtRuntimeEditorWindow.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "editor/RuntimeEditor.h"

namespace Play::editor
{

namespace
{
QString toQString(const std::string& text)
{
    return QString::fromStdString(text);
}

std::string toStdString(const QString& text)
{
    return text.toStdString();
}

QString makeDoubleText(double value)
{
    return QString::number(value, 'g', 9);
}

double textToDouble(const std::string& text, double fallback = 0.0)
{
    bool         ok    = false;
    const double value = QString::fromStdString(text).toDouble(&ok);
    return ok ? value : fallback;
}

const EditorUiSceneNode* findSceneNode(const EditorUiSceneNode& node, const std::string& key)
{
    if (node.key == key)
    {
        return &node;
    }

    for (const EditorUiSceneNode& child : node.children)
    {
        const EditorUiSceneNode* found = findSceneNode(child, key);
        if (found)
        {
            return found;
        }
    }

    return nullptr;
}

void addSeparator(QVBoxLayout* layout)
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);
}

QLabel* makeMutedLabel(const QString& text)
{
    QLabel* label = new QLabel(text);
    label->setWordWrap(true);
    label->setProperty("muted", true);
    return label;
}

void applySpinBoxRange(QDoubleSpinBox* spinBox, const EditorUiProperty& property)
{
    spinBox->setRange(-1000000000.0, 1000000000.0);
    if (property.hasMinimum)
    {
        spinBox->setMinimum(textToDouble(property.minimum, spinBox->minimum()));
    }
    if (property.hasMaximum)
    {
        spinBox->setMaximum(textToDouble(property.maximum, spinBox->maximum()));
    }
    spinBox->setSingleStep(textToDouble(property.step, 1.0));
}
} // namespace

QtRuntimeEditorWindow::QtRuntimeEditorWindow(RuntimeEditor& editor, QWidget* parent) : QMainWindow(parent), _editor(editor)
{
    setWindowTitle("VulkanPlayGround Control Panel");
    resize(1120, 760);

    _tabs = new QTabWidget(this);
    _tabs->setDocumentMode(true);
    setCentralWidget(_tabs);

    QObject::connect(_tabs, &QTabWidget::currentChanged, this,
                     [this](int index)
                     {
                         if (_refreshing || index < 0)
                         {
                             return;
                         }

                         QWidget*          page   = _tabs->widget(index);
                         const std::string modeId = page ? toStdString(page->property("renderModeId").toString()) : std::string();
                         if (modeId.empty())
                         {
                             return;
                         }

                         if (_editor.getRenderModeTabs().requestActiveMode(modeId.c_str()))
                         {
                             _currentRenderMode = modeId;
                         }
                         else
                         {
                             scheduleRefresh();
                         }
                     });

    _refreshTimer = new QTimer(this);
    QObject::connect(_refreshTimer, &QTimer::timeout, this, [this]() { refreshFromEditor(); });
    _refreshTimer->start(500);

    refreshFromEditor();
}

void QtRuntimeEditorWindow::refreshFromEditor()
{
    if (_refreshing)
    {
        return;
    }

    _refreshing = true;
    const QSignalBlocker blocker(_tabs);

    const EditorUiSnapshot snapshot = _editor.buildSnapshot();

    if (_currentRenderMode.empty())
    {
        for (const EditorUiRenderMode& renderMode : snapshot.renderModes)
        {
            if (renderMode.active)
            {
                _currentRenderMode = renderMode.id;
                break;
            }
        }
    }

    _tabs->clear();

    int currentIndex = 0;
    for (const EditorUiRenderMode& renderMode : snapshot.renderModes)
    {
        QWidget* page = buildRenderModePage(renderMode);
        page->setProperty("renderModeId", toQString(renderMode.id));
        const int index = _tabs->addTab(page, toQString(renderMode.title));

        if (renderMode.id == _currentRenderMode || (_currentRenderMode.empty() && renderMode.active))
        {
            currentIndex = index;
        }
    }

    if (_tabs->count() > 0)
    {
        _tabs->setCurrentIndex(currentIndex);
    }

    _refreshing = false;
}

QWidget* QtRuntimeEditorWindow::buildRenderModePage(const EditorUiRenderMode& renderMode)
{
    QWidget* page = new QWidget();

    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(8, 8, 8, 8);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, page);
    splitter->addWidget(buildScenePanel(renderMode));

    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, splitter);
    rightSplitter->addWidget(buildInspectorPanel(renderMode));
    rightSplitter->addWidget(buildControlsPanel(renderMode));
    rightSplitter->setStretchFactor(0, 2);
    rightSplitter->setStretchFactor(1, 3);

    splitter->addWidget(rightSplitter);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);
    pageLayout->addWidget(splitter);

    return page;
}

QWidget* QtRuntimeEditorWindow::buildScenePanel(const EditorUiRenderMode& renderMode)
{
    QWidget* panel = new QWidget();

    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* header = new QHBoxLayout();
    QLabel*      title  = new QLabel("Scene Tree");
    title->setProperty("sectionTitle", true);
    header->addWidget(title);
    header->addStretch();

    QPushButton* add2D = new QPushButton("Node2D");
    QPushButton* add3D = new QPushButton("Node3D");
    add2D->setEnabled(renderMode.scene.available);
    add3D->setEnabled(renderMode.scene.available);
    header->addWidget(add2D);
    header->addWidget(add3D);
    layout->addLayout(header);

    QObject::connect(add2D, &QPushButton::clicked, this, [this, id = renderMode.id]() { requestCreateSceneNode(id, "Node2D"); });
    QObject::connect(add3D, &QPushButton::clicked, this, [this, id = renderMode.id]() { requestCreateSceneNode(id, "Node3D"); });

    if (!renderMode.scene.available)
    {
        layout->addWidget(makeMutedLabel(toQString(renderMode.scene.emptyText)));
        layout->addStretch();
        return panel;
    }

    QTreeWidget* tree = new QTreeWidget(panel);
    tree->setColumnCount(2);
    tree->setHeaderLabels(QStringList() << "Node" << "Type");
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(tree);

    std::map<std::string, QTreeWidgetItem*> itemsByKey;

    const auto addNode = [&](const auto& self, const EditorUiSceneNode& node, QTreeWidgetItem* parent) -> void
    {
        QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
        item->setText(0, toQString(node.name));
        item->setText(1, node.is3D ? "3D" : "2D");
        item->setData(0, Qt::UserRole, toQString(node.key));
        itemsByKey[node.key] = item;

        const bool expanded = parent == nullptr || _expandedNodesByMode[renderMode.id].count(node.key) > 0;
        item->setExpanded(expanded);

        for (const EditorUiSceneNode& child : node.children)
        {
            self(self, child, item);
        }
    };

    addNode(addNode, renderMode.scene.root, nullptr);

    std::string selectedKey = _selectedNodeByMode[renderMode.id];
    if (selectedKey.empty() || !findSceneNode(renderMode.scene.root, selectedKey))
    {
        selectedKey                        = renderMode.scene.root.key;
        _selectedNodeByMode[renderMode.id] = selectedKey;
    }

    const auto selectedIt = itemsByKey.find(selectedKey);
    if (selectedIt != itemsByKey.end())
    {
        tree->setCurrentItem(selectedIt->second);
    }

    QObject::connect(tree, &QTreeWidget::itemExpanded, this, [this, id = renderMode.id](QTreeWidgetItem* item)
                     { _expandedNodesByMode[id].insert(toStdString(item->data(0, Qt::UserRole).toString())); });
    QObject::connect(tree, &QTreeWidget::itemCollapsed, this, [this, id = renderMode.id](QTreeWidgetItem* item)
                     { _expandedNodesByMode[id].erase(toStdString(item->data(0, Qt::UserRole).toString())); });
    QObject::connect(tree, &QTreeWidget::itemSelectionChanged, this,
                     [this, tree, id = renderMode.id]()
                     {
                         QTreeWidgetItem* item = tree->currentItem();
                         if (!item)
                         {
                             return;
                         }
                         _selectedNodeByMode[id] = toStdString(item->data(0, Qt::UserRole).toString());
                         QTimer::singleShot(0, this, [this]() { refreshFromEditor(); });
                     });

    return panel;
}

QWidget* QtRuntimeEditorWindow::buildInspectorPanel(const EditorUiRenderMode& renderMode)
{
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    QWidget* content = new QWidget();
    scroll->setWidget(content);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel* title = new QLabel("Node Inspector");
    title->setProperty("sectionTitle", true);
    layout->addWidget(title);

    if (!renderMode.scene.available)
    {
        layout->addWidget(makeMutedLabel(toQString(renderMode.scene.emptyText)));
        layout->addStretch();
        return scroll;
    }

    std::string selectedKey = _selectedNodeByMode[renderMode.id];
    if (selectedKey.empty())
    {
        selectedKey = renderMode.scene.root.key;
    }

    const EditorUiSceneNode* node = findSceneNode(renderMode.scene.root, selectedKey);
    if (!node)
    {
        node                               = &renderMode.scene.root;
        _selectedNodeByMode[renderMode.id] = node->key;
    }

    QLabel* name = new QLabel(toQString(node->name));
    name->setProperty("objectTitle", true);
    layout->addWidget(name);
    layout->addWidget(makeMutedLabel(toQString(node->typeName + "  " + node->key)));
    addSeparator(layout);

    QGroupBox*   transformGroup = new QGroupBox("Transform");
    QFormLayout* transformForm  = new QFormLayout(transformGroup);

    const auto addTransformRow = [&](const char* label, const char* property, const float values[3], double step)
    {
        QWidget*     row       = new QWidget();
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        static const char* components[] = {"x", "y", "z"};
        for (int componentIndex = 0; componentIndex < 3; ++componentIndex)
        {
            QDoubleSpinBox* spin = new QDoubleSpinBox(row);
            spin->setDecimals(4);
            spin->setRange(-1000000000.0, 1000000000.0);
            spin->setSingleStep(step);
            spin->setKeyboardTracking(false);
            spin->setValue(values[componentIndex]);
            rowLayout->addWidget(spin);

            const std::string path = std::string(property) + "." + components[componentIndex];
            QObject::connect(spin, &QDoubleSpinBox::valueChanged, this,
                             [this, id = renderMode.id, key = node->key, path](double value)
                             {
                                 if (!_refreshing)
                                 {
                                     requestSetSceneNodeTransform(id, key, path.c_str(), value);
                                 }
                             });
        }

        transformForm->addRow(label, row);
    };

    addTransformRow("Translation", "translation", node->transform.translation, 0.01);
    addTransformRow("Rotation", "rotation", node->transform.rotation, 0.1);
    addTransformRow("Scale", "scale", node->transform.scale, 0.01);
    layout->addWidget(transformGroup);

    QGroupBox*   componentGroup  = new QGroupBox("Components");
    QVBoxLayout* componentLayout = new QVBoxLayout(componentGroup);

    QPushButton* addModel = new QPushButton("Add ModelComponent");
    addModel->setEnabled(node->canAddModelComponent);
    componentLayout->addWidget(addModel);
    QObject::connect(addModel, &QPushButton::clicked, this, [this, id = renderMode.id, key = node->key]() { requestAddSceneNodeComponent(id, key); });

    if (node->components.empty())
    {
        componentLayout->addWidget(makeMutedLabel("No components."));
    }

    for (const EditorUiSceneComponent& component : node->components)
    {
        QGroupBox*   componentBox  = new QGroupBox(toQString(component.typeName));
        QFormLayout* componentForm = new QFormLayout(componentBox);
        for (const EditorUiKeyValue& detail : component.details)
        {
            QLabel* value = new QLabel(toQString(detail.value));
            value->setWordWrap(true);
            componentForm->addRow(toQString(detail.label), value);
        }
        componentLayout->addWidget(componentBox);
    }
    layout->addWidget(componentGroup);
    layout->addStretch();

    return scroll;
}

QWidget* QtRuntimeEditorWindow::buildControlsPanel(const EditorUiRenderMode& renderMode)
{
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    QWidget* content = new QWidget();
    scroll->setWidget(content);

    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel* title = new QLabel("Control Panel");
    title->setProperty("sectionTitle", true);
    layout->addWidget(title);

    if (renderMode.controls.empty())
    {
        layout->addWidget(makeMutedLabel("No control objects registered for this render mode."));
        layout->addStretch();
        return scroll;
    }

    for (const EditorUiObject& object : renderMode.controls)
    {
        QGroupBox*   group       = new QGroupBox(toQString(object.title));
        QVBoxLayout* groupLayout = new QVBoxLayout(group);

        QHBoxLayout* objectHeader = new QHBoxLayout();
        objectHeader->addWidget(makeMutedLabel(toQString(object.typeName)));
        objectHeader->addStretch();
        QPushButton* reset = new QPushButton("Reset");
        reset->setEnabled(object.canReset);
        objectHeader->addWidget(reset);
        groupLayout->addLayout(objectHeader);
        QObject::connect(reset, &QPushButton::clicked, this, [this, id = object.id]() { requestResetObject(id); });

        QFormLayout* form = new QFormLayout();
        groupLayout->addLayout(form);

        if (object.properties.empty())
        {
            groupLayout->addWidget(makeMutedLabel("No reflected properties."));
        }

        for (const EditorUiProperty& property : object.properties)
        {
            if (property.kind == EditorUiPropertyKind::Bool)
            {
                QCheckBox* check = new QCheckBox();
                check->setEnabled(property.editable);
                check->setChecked(property.boolValue);
                QObject::connect(check, &QCheckBox::toggled, this, [this, id = object.id, path = property.path](bool checked)
                                 { requestSetObjectProperty(id, path, checked ? "true" : "false"); });
                form->addRow(toQString(property.label), check);
            }
            else if (property.kind == EditorUiPropertyKind::Number)
            {
                QDoubleSpinBox* spin = new QDoubleSpinBox();
                spin->setDecimals(6);
                spin->setKeyboardTracking(false);
                spin->setEnabled(property.editable);
                applySpinBoxRange(spin, property);
                spin->setValue(textToDouble(property.value));
                QObject::connect(spin, &QDoubleSpinBox::valueChanged, this, [this, id = object.id, path = property.path](double value)
                                 { requestSetObjectProperty(id, path, toStdString(makeDoubleText(value))); });
                form->addRow(toQString(property.label), spin);
            }
            else
            {
                QLineEdit* edit = new QLineEdit(toQString(property.value));
                edit->setReadOnly(!property.editable);
                QObject::connect(edit, &QLineEdit::editingFinished, this, [this, edit, id = object.id, path = property.path]()
                                 { requestSetObjectProperty(id, path, toStdString(edit->text())); });
                form->addRow(toQString(property.label), edit);
            }
        }

        layout->addWidget(group);
    }

    layout->addStretch();
    return scroll;
}

void QtRuntimeEditorWindow::requestCreateSceneNode(const std::string& renderModeId, const char* nodeType)
{
    const std::string parentNode = _selectedNodeByMode[renderModeId];
    const std::string nodeKey    = _editor.getRenderModeTabs().createSceneNode(renderModeId.c_str(), parentNode.c_str(), nodeType);
    if (!nodeKey.empty())
    {
        _selectedNodeByMode[renderModeId] = nodeKey;
        _expandedNodesByMode[renderModeId].insert(parentNode);
    }
    scheduleRefresh();
}

void QtRuntimeEditorWindow::requestSetSceneNodeTransform(const std::string& renderModeId, const std::string& nodeKey, const char* path, double value)
{
    _editor.getRenderModeTabs().setSceneNodeTransform(renderModeId.c_str(), nodeKey.c_str(), path, toStdString(makeDoubleText(value)).c_str());
    scheduleRefresh();
}

void QtRuntimeEditorWindow::requestAddSceneNodeComponent(const std::string& renderModeId, const std::string& nodeKey)
{
    _editor.getRenderModeTabs().addSceneNodeComponent(renderModeId.c_str(), nodeKey.c_str(), "ModelComponent");
    scheduleRefresh();
}

void QtRuntimeEditorWindow::requestSetObjectProperty(unsigned int objectId, const std::string& propertyPath, const std::string& value)
{
    _editor.getEditorRegistry().setObjectProperty(objectId, propertyPath.c_str(), rttr::variant(value));
    scheduleRefresh();
}

void QtRuntimeEditorWindow::requestResetObject(unsigned int objectId)
{
    _editor.getEditorRegistry().resetObject(objectId);
    scheduleRefresh();
}

void QtRuntimeEditorWindow::scheduleRefresh()
{
    QTimer::singleShot(0, this, [this]() { refreshFromEditor(); });
}

} // namespace Play::editor
