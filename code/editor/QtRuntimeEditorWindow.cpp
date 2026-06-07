#include "editor/QtRuntimeEditorWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "editor/RuntimeEditor.h"
#include "editor/SceneInspectorPanel.h"

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
    return QString::number(value, 'f', 3);
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

QLabel* makeMutedLabel(const QString& text)
{
    QLabel* label = new QLabel(text);
    label->setWordWrap(true);
    label->setProperty("muted", true);
    return label;
}

bool widgetContainsFocus(const QWidget* widget)
{
    const QWidget* focusWidget = QApplication::focusWidget();
    return widget && focusWidget && (widget == focusWidget || widget->isAncestorOf(focusWidget));
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

std::string itemKey(QTreeWidgetItem* item)
{
    return item ? toStdString(item->data(0, Qt::UserRole).toString()) : std::string();
}

int controlPanelColumnCount(const QScrollArea* scrollArea)
{
    const int viewportWidth = scrollArea && scrollArea->viewport() ? scrollArea->viewport()->width() : 0;
    return viewportWidth >= 760 ? 2 : 1;
}

struct VectorComponentInfo
{
    std::string rootPath;
    int         componentIndex = -1;
};

int vectorComponentIndex(const std::string& component)
{
    if (component == "x" || component == "r")
    {
        return 0;
    }

    if (component == "y" || component == "g")
    {
        return 1;
    }

    if (component == "z" || component == "b")
    {
        return 2;
    }

    if (component == "w" || component == "a")
    {
        return 3;
    }

    return -1;
}

bool parseVectorComponent(const EditorUiProperty& property, VectorComponentInfo& output)
{
    if (property.kind != EditorUiPropertyKind::Number)
    {
        return false;
    }

    const size_t dot = property.path.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= property.path.size())
    {
        return false;
    }

    const int componentIndex = vectorComponentIndex(property.path.substr(dot + 1));
    if (componentIndex < 0)
    {
        return false;
    }

    output.rootPath       = property.path.substr(0, dot);
    output.componentIndex = componentIndex;
    return true;
}

std::string makeVectorLabel(const EditorUiProperty& property, const std::string& rootPath)
{
    const size_t dot = property.label.rfind('.');
    if (dot != std::string::npos && vectorComponentIndex(property.label.substr(dot + 1)) >= 0)
    {
        return property.label.substr(0, dot);
    }

    return rootPath;
}

class SceneTreeWidget final : public QTreeWidget
{
public:
    explicit SceneTreeWidget(QWidget* parent = nullptr) : QTreeWidget(parent) {}

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (!itemAt(event->position().toPoint()))
        {
            setCurrentItem(nullptr);
            clearSelection();
            event->accept();
            return;
        }
        QTreeWidget::mousePressEvent(event);
    }
};
} // namespace

struct QtRuntimeEditorWindow::RenderModePage
{
    struct PropertyWidgets
    {
        QLabel*         label  = nullptr;
        QWidget*        editor = nullptr;
        QDoubleSpinBox* componentEditors[4] = {};
        std::string     componentPaths[4];
        int             kind           = -1;
        int             componentCount = 0;
        bool            seen           = false;
    };

    struct ControlObjectWidgets
    {
        QGroupBox*                           group           = nullptr;
        QLabel*                              typeLabel       = nullptr;
        QPushButton*                         resetButton     = nullptr;
        QFormLayout*                         form            = nullptr;
        QLabel*                              emptyLabel      = nullptr;
        bool                                 seen            = false;
        std::map<std::string, PropertyWidgets> propertyWidgets;
    };

    std::string renderModeId;
    bool        seen              = false;
    bool        updatingSceneTree = false;

    QWidget*    page = nullptr;
    QTreeWidget* sceneTree = nullptr;
    QLabel*     sceneEmptyLabel = nullptr;
    QPushButton* addNode2DButton = nullptr;
    QPushButton* addNode3DButton = nullptr;

    QScrollArea*         inspectorScroll = nullptr;
    SceneInspectorPanel* inspectorPanel  = nullptr;
    std::string          inspectorNodeKey;

    QScrollArea* controlsScroll = nullptr;
    QWidget*     controlsContent = nullptr;
    QVBoxLayout* controlsLayout = nullptr;
    QWidget*     controlsGridHost = nullptr;
    QGridLayout* controlsGrid = nullptr;
    QLabel*      controlsEmptyLabel = nullptr;

    std::map<unsigned int, ControlObjectWidgets> controlObjects;
    std::map<std::string, QTreeWidgetItem*>      sceneItemsByKey;
};

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

QtRuntimeEditorWindow::~QtRuntimeEditorWindow()
{
    if (_refreshTimer)
    {
        _refreshTimer->stop();
    }
    _refreshScheduled = false;

    for (auto& pageEntry : _pagesByMode)
    {
        delete pageEntry.second;
    }
    _pagesByMode.clear();
}

void QtRuntimeEditorWindow::refreshFromEditor()
{
    if (_refreshing)
    {
        return;
    }

    _refreshScheduled = false;
    _refreshing = true;
    const EditorUiSnapshot snapshot = _editor.buildSnapshot();
    updateRenderModeTabs(snapshot);
    _refreshing = false;
}

QtRuntimeEditorWindow::RenderModePage* QtRuntimeEditorWindow::createRenderModePage(const EditorUiRenderMode& renderMode)
{
    RenderModePage* page = new RenderModePage();
    page->renderModeId  = renderMode.id;
    page->page          = new QWidget();
    page->page->setProperty("renderModeId", toQString(renderMode.id));

    QVBoxLayout* pageLayout = new QVBoxLayout(page->page);
    pageLayout->setContentsMargins(8, 8, 8, 8);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, page->page);
    QWidget*   scenePanel = new QWidget(splitter);
    QVBoxLayout* sceneLayout = new QVBoxLayout(scenePanel);
    sceneLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* sceneHeader = new QHBoxLayout();
    QLabel*      sceneTitle  = new QLabel("Scene Tree");
    sceneTitle->setProperty("sectionTitle", true);
    sceneHeader->addWidget(sceneTitle);
    sceneHeader->addStretch();

    page->addNode2DButton = new QPushButton("Node2D");
    page->addNode3DButton = new QPushButton("Node3D");
    sceneHeader->addWidget(page->addNode2DButton);
    sceneHeader->addWidget(page->addNode3DButton);
    sceneLayout->addLayout(sceneHeader);

    page->sceneEmptyLabel = makeMutedLabel("");
    sceneLayout->addWidget(page->sceneEmptyLabel);

    page->sceneTree = new SceneTreeWidget(scenePanel);
    page->sceneTree->setColumnCount(2);
    page->sceneTree->setHeaderLabels(QStringList() << "Node" << "Type");
    page->sceneTree->header()->setStretchLastSection(false);
    page->sceneTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    page->sceneTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    sceneLayout->addWidget(page->sceneTree);

    QObject::connect(page->addNode2DButton, &QPushButton::clicked, this,
                     [this, page]() { requestCreateSceneNode(page->renderModeId, "Node2D"); });
    QObject::connect(page->addNode3DButton, &QPushButton::clicked, this,
                     [this, page]() { requestCreateSceneNode(page->renderModeId, "Node3D"); });
    QObject::connect(page->sceneTree, &QTreeWidget::itemExpanded, this,
                     [this, page](QTreeWidgetItem* item)
                     {
                         if (!page->updatingSceneTree)
                         {
                             _expandedNodesByMode[page->renderModeId].insert(itemKey(item));
                         }
                     });
    QObject::connect(page->sceneTree, &QTreeWidget::itemCollapsed, this,
                     [this, page](QTreeWidgetItem* item)
                     {
                         if (!page->updatingSceneTree)
                         {
                             _expandedNodesByMode[page->renderModeId].erase(itemKey(item));
                         }
                     });
    QObject::connect(page->sceneTree, &QTreeWidget::currentItemChanged, this,
                     [this, page](QTreeWidgetItem* item, QTreeWidgetItem*)
                     {
                         if (page->updatingSceneTree)
                         {
                             return;
                         }

                         if (item)
                         {
                             _selectedNodeByMode[page->renderModeId] = itemKey(item);
                         }
                         else
                         {
                             _selectedNodeByMode[page->renderModeId].clear();
                         }
                         scheduleRefresh();
                     });

    splitter->addWidget(scenePanel);

    QSplitter* rightSplitter = new QSplitter(Qt::Vertical, splitter);

    page->inspectorScroll = new QScrollArea(rightSplitter);
    page->inspectorScroll->setWidgetResizable(true);
    page->inspectorPanel = new SceneInspectorPanel();
    page->inspectorScroll->setWidget(page->inspectorPanel);

    QObject::connect(page->inspectorPanel, &SceneInspectorPanel::sceneNodeTransformRequested, this,
                     [this](const QString& renderModeId, const QString& nodeKey, const QString& path, double value)
                     {
                         if (!_refreshing)
                         {
                             const std::string renderMode = toStdString(renderModeId);
                             const std::string node       = toStdString(nodeKey);
                             const std::string transform  = toStdString(path);
                             requestSetSceneNodeTransform(renderMode, node, transform.c_str(), value);
                         }
                     });
    QObject::connect(page->inspectorPanel, &SceneInspectorPanel::sceneNodeComponentRequested, this,
                     [this](const QString& renderModeId, const QString& nodeKey, const QString& componentType)
                     {
                         if (!_refreshing)
                         {
                             requestAddSceneNodeComponent(toStdString(renderModeId), toStdString(nodeKey), toStdString(componentType));
                         }
                     });
    QObject::connect(page->inspectorPanel, &SceneInspectorPanel::sceneNodeModelLoadRequested, this,
                     [this](const QString& renderModeId, const QString& nodeKey, const QString& path)
                     {
                         if (!_refreshing)
                         {
                             requestLoadSceneNodeModel(toStdString(renderModeId), toStdString(nodeKey), toStdString(path));
                         }
                     });

    rightSplitter->addWidget(page->inspectorScroll);

    page->controlsScroll = new QScrollArea(rightSplitter);
    page->controlsScroll->setWidgetResizable(true);
    page->controlsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    page->controlsContent = new QWidget();
    page->controlsContent->setMinimumWidth(0);
    page->controlsScroll->setWidget(page->controlsContent);
    page->controlsLayout = new QVBoxLayout(page->controlsContent);
    page->controlsLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* controlsTitle = new QLabel("Control Panel");
    controlsTitle->setProperty("sectionTitle", true);
    page->controlsLayout->addWidget(controlsTitle);
    page->controlsEmptyLabel = makeMutedLabel("No control objects registered for this render mode.");
    page->controlsLayout->addWidget(page->controlsEmptyLabel);
    page->controlsGridHost = new QWidget(page->controlsContent);
    page->controlsGridHost->setMinimumWidth(0);
    page->controlsGrid = new QGridLayout(page->controlsGridHost);
    page->controlsGrid->setContentsMargins(0, 0, 0, 0);
    page->controlsGrid->setColumnStretch(0, 1);
    page->controlsGrid->setColumnStretch(1, 1);
    page->controlsLayout->addWidget(page->controlsGridHost);
    page->controlsLayout->addStretch();

    rightSplitter->addWidget(page->controlsScroll);
    rightSplitter->setStretchFactor(0, 2);
    rightSplitter->setStretchFactor(1, 3);

    splitter->addWidget(rightSplitter);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);
    pageLayout->addWidget(splitter);

    return page;
}

void QtRuntimeEditorWindow::updateRenderModeTabs(const EditorUiSnapshot& snapshot)
{
    const QSignalBlocker blocker(_tabs);

    for (auto& pageEntry : _pagesByMode)
    {
        pageEntry.second->seen = false;
    }

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

    for (const EditorUiRenderMode& renderMode : snapshot.renderModes)
    {
        RenderModePage* page = nullptr;
        auto            pageIt = _pagesByMode.find(renderMode.id);
        if (pageIt == _pagesByMode.end())
        {
            page = createRenderModePage(renderMode);
            _pagesByMode[renderMode.id] = page;
            _tabs->addTab(page->page, toQString(renderMode.title));
        }
        else
        {
            page = pageIt->second;
        }

        page->seen = true;
        page->page->setProperty("renderModeId", toQString(renderMode.id));
        const int tabIndex = _tabs->indexOf(page->page);
        if (tabIndex >= 0)
        {
            _tabs->setTabText(tabIndex, toQString(renderMode.title));
        }
        updateRenderModePage(*page, renderMode);
    }

    for (auto pageIt = _pagesByMode.begin(); pageIt != _pagesByMode.end();)
    {
        RenderModePage* page = pageIt->second;
        if (page->seen)
        {
            ++pageIt;
            continue;
        }

        const int tabIndex = _tabs->indexOf(page->page);
        if (tabIndex >= 0)
        {
            _tabs->removeTab(tabIndex);
        }
        delete page->page;
        delete page;
        pageIt = _pagesByMode.erase(pageIt);
    }

    int currentIndex = -1;
    for (const EditorUiRenderMode& renderMode : snapshot.renderModes)
    {
        auto pageIt = _pagesByMode.find(renderMode.id);
        if (pageIt == _pagesByMode.end())
        {
            continue;
        }

        const int tabIndex = _tabs->indexOf(pageIt->second->page);
        if (renderMode.id == _currentRenderMode)
        {
            currentIndex = tabIndex;
        }
    }

    if (currentIndex < 0)
    {
        for (const EditorUiRenderMode& renderMode : snapshot.renderModes)
        {
            auto pageIt = _pagesByMode.find(renderMode.id);
            if (pageIt != _pagesByMode.end())
            {
                currentIndex        = _tabs->indexOf(pageIt->second->page);
                _currentRenderMode = renderMode.id;
                if (renderMode.active)
                {
                    break;
                }
            }
        }
    }

    if (currentIndex >= 0)
    {
        _tabs->setCurrentIndex(currentIndex);
    }
}

void QtRuntimeEditorWindow::updateRenderModePage(RenderModePage& page, const EditorUiRenderMode& renderMode)
{
    updateScenePanel(page, renderMode);
    updateInspectorPanel(page, renderMode);
    updateControlsPanel(page, renderMode);
}

void QtRuntimeEditorWindow::updateScenePanel(RenderModePage& page, const EditorUiRenderMode& renderMode)
{
    page.addNode2DButton->setEnabled(renderMode.scene.available);
    page.addNode3DButton->setEnabled(renderMode.scene.available);

    if (!renderMode.scene.available)
    {
        page.sceneEmptyLabel->setText(toQString(renderMode.scene.emptyText));
        page.sceneEmptyLabel->show();
        page.sceneTree->hide();
        _selectedNodeByMode[renderMode.id].clear();
        clearSceneTree(page);
        return;
    }

    page.sceneEmptyLabel->hide();
    page.sceneTree->show();

    page.updatingSceneTree = true;
    const QSignalBlocker blocker(page.sceneTree);
    syncSceneTreeNode(page, renderMode.scene.root, nullptr, 0);
    while (page.sceneTree->topLevelItemCount() > 1)
    {
        QTreeWidgetItem* item = page.sceneTree->takeTopLevelItem(1);
        removeSceneTreeItem(page, item);
    }

    std::string& selectedKey = _selectedNodeByMode[renderMode.id];
    if (!selectedKey.empty() && !findSceneNode(renderMode.scene.root, selectedKey))
    {
        selectedKey.clear();
    }

    if (selectedKey.empty())
    {
        page.sceneTree->clearSelection();
        page.sceneTree->setCurrentItem(nullptr);
    }
    else
    {
        auto itemIt = page.sceneItemsByKey.find(selectedKey);
        if (itemIt != page.sceneItemsByKey.end())
        {
            page.sceneTree->setCurrentItem(itemIt->second);
        }
    }
    page.updatingSceneTree = false;
}

QTreeWidgetItem* QtRuntimeEditorWindow::syncSceneTreeNode(RenderModePage& page, const EditorUiSceneNode& node, QTreeWidgetItem* parent, int row)
{
    QTreeWidgetItem* item = nullptr;
    auto             itemIt = page.sceneItemsByKey.find(node.key);
    if (itemIt == page.sceneItemsByKey.end())
    {
        item = new QTreeWidgetItem();
        item->setData(0, Qt::UserRole, toQString(node.key));
        page.sceneItemsByKey[node.key] = item;
    }
    else
    {
        item = itemIt->second;
    }

    if (parent)
    {
        if (item->parent() != parent || parent->indexOfChild(item) != row)
        {
            if (item->parent())
            {
                item->parent()->takeChild(item->parent()->indexOfChild(item));
            }
            else
            {
                const int oldTopLevelIndex = page.sceneTree->indexOfTopLevelItem(item);
                if (oldTopLevelIndex >= 0)
                {
                    page.sceneTree->takeTopLevelItem(oldTopLevelIndex);
                }
            }
            parent->insertChild(row, item);
        }
    }
    else if (page.sceneTree->indexOfTopLevelItem(item) != row)
    {
        if (item->parent())
        {
            item->parent()->takeChild(item->parent()->indexOfChild(item));
        }
        else
        {
            const int oldTopLevelIndex = page.sceneTree->indexOfTopLevelItem(item);
            if (oldTopLevelIndex >= 0)
            {
                page.sceneTree->takeTopLevelItem(oldTopLevelIndex);
            }
        }
        page.sceneTree->insertTopLevelItem(row, item);
    }

    item->setText(0, toQString(node.name));
    item->setText(1, node.is3D ? "3D" : "2D");
    item->setData(0, Qt::UserRole, toQString(node.key));

    const bool expanded = !parent || _expandedNodesByMode[page.renderModeId].count(node.key) > 0;
    item->setExpanded(expanded);

    int childRow = 0;
    for (const EditorUiSceneNode& child : node.children)
    {
        syncSceneTreeNode(page, child, item, childRow);
        ++childRow;
    }

    while (item->childCount() > childRow)
    {
        QTreeWidgetItem* child = item->takeChild(childRow);
        removeSceneTreeItem(page, child);
    }

    return item;
}

void QtRuntimeEditorWindow::removeSceneTreeItem(RenderModePage& page, QTreeWidgetItem* item)
{
    if (!item)
    {
        return;
    }

    while (item->childCount() > 0)
    {
        QTreeWidgetItem* child = item->takeChild(0);
        removeSceneTreeItem(page, child);
    }

    page.sceneItemsByKey.erase(itemKey(item));
    delete item;
}

void QtRuntimeEditorWindow::clearSceneTree(RenderModePage& page)
{
    page.updatingSceneTree = true;
    while (page.sceneTree->topLevelItemCount() > 0)
    {
        QTreeWidgetItem* item = page.sceneTree->takeTopLevelItem(0);
        removeSceneTreeItem(page, item);
    }
    page.updatingSceneTree = false;
}

void QtRuntimeEditorWindow::updateInspectorPanel(RenderModePage& page, const EditorUiRenderMode& renderMode)
{
    if (!renderMode.scene.available)
    {
        page.inspectorNodeKey.clear();
        page.inspectorPanel->setUnavailable(toQString(renderMode.id), toQString(renderMode.scene.emptyText));
        page.inspectorScroll->verticalScrollBar()->setValue(0);
        return;
    }

    const std::string selectedKey = _selectedNodeByMode[renderMode.id];
    if (selectedKey.empty())
    {
        page.inspectorNodeKey.clear();
        page.inspectorPanel->setEmpty(toQString(renderMode.id), "No scene node selected.");
        page.inspectorScroll->verticalScrollBar()->setValue(0);
        return;
    }

    const EditorUiSceneNode* node = findSceneNode(renderMode.scene.root, selectedKey);
    if (!node)
    {
        _selectedNodeByMode[renderMode.id].clear();
        page.inspectorNodeKey.clear();
        page.inspectorPanel->setEmpty(toQString(renderMode.id), "No scene node selected.");
        page.inspectorScroll->verticalScrollBar()->setValue(0);
        return;
    }

    const bool selectedNodeChanged = page.inspectorNodeKey != node->key;
    page.inspectorNodeKey = node->key;
    if (selectedNodeChanged)
    {
        page.inspectorScroll->verticalScrollBar()->setValue(0);
    }
    page.inspectorPanel->setNode(toQString(renderMode.id), *node);
}

void QtRuntimeEditorWindow::updateControlsPanel(RenderModePage& page, const EditorUiRenderMode& renderMode)
{
    for (auto& objectEntry : page.controlObjects)
    {
        objectEntry.second.seen = false;
    }

    page.controlsEmptyLabel->setVisible(renderMode.controls.empty());
    page.controlsGridHost->setVisible(!renderMode.controls.empty());
    const int columnCount = controlPanelColumnCount(page.controlsScroll);
    page.controlsGrid->setColumnStretch(0, 1);
    page.controlsGrid->setColumnStretch(1, columnCount > 1 ? 1 : 0);
    int controlIndex = 0;
    for (const EditorUiObject& object : renderMode.controls)
    {
        updateControlObject(page, object);
        RenderModePage::ControlObjectWidgets& widgets = page.controlObjects[object.id];
        page.controlsGrid->addWidget(widgets.group, controlIndex / columnCount, controlIndex % columnCount);
        ++controlIndex;
    }

    removeUnseenControlObjects(page);
}

void QtRuntimeEditorWindow::updateControlObject(RenderModePage& page, const EditorUiObject& object)
{
    RenderModePage::ControlObjectWidgets& widgets = page.controlObjects[object.id];
    if (!widgets.group)
    {
        widgets.group = new QGroupBox(page.controlsGridHost);
        widgets.group->setMinimumWidth(0);
        QVBoxLayout* groupLayout = new QVBoxLayout(widgets.group);

        QHBoxLayout* objectHeader = new QHBoxLayout();
        widgets.typeLabel = makeMutedLabel("");
        objectHeader->addWidget(widgets.typeLabel);
        objectHeader->addStretch();
        widgets.resetButton = new QPushButton("Reset");
        objectHeader->addWidget(widgets.resetButton);
        groupLayout->addLayout(objectHeader);

        widgets.form = new QFormLayout();
        widgets.form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        widgets.form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        groupLayout->addLayout(widgets.form);
        widgets.emptyLabel = makeMutedLabel("No reflected properties.");
        groupLayout->addWidget(widgets.emptyLabel);
        QObject::connect(widgets.resetButton, &QPushButton::clicked, this, [this, id = object.id]() { requestResetObject(id); });
    }

    widgets.seen = true;
    widgets.group->setTitle(toQString(object.title));
    widgets.typeLabel->setText(toQString(object.typeName));
    widgets.resetButton->setEnabled(object.canReset);

    for (auto& propertyEntry : widgets.propertyWidgets)
    {
        propertyEntry.second.seen = false;
    }

    widgets.emptyLabel->setVisible(object.properties.empty());
    for (size_t propertyIndex = 0; propertyIndex < object.properties.size();)
    {
        const EditorUiProperty& property = object.properties[propertyIndex];

        VectorComponentInfo vectorInfo;
        if (parseVectorComponent(property, vectorInfo))
        {
            size_t propertyCount = 0;
            while (propertyIndex + propertyCount < object.properties.size())
            {
                VectorComponentInfo nextVectorInfo;
                if (!parseVectorComponent(object.properties[propertyIndex + propertyCount], nextVectorInfo) ||
                    nextVectorInfo.rootPath != vectorInfo.rootPath)
                {
                    break;
                }
                ++propertyCount;
            }

            if (propertyCount > 1)
            {
                updateVectorPropertyWidget(page, object.id, object, propertyIndex, propertyCount, vectorInfo.rootPath,
                                           makeVectorLabel(property, vectorInfo.rootPath));
                propertyIndex += propertyCount;
                continue;
            }
        }

        updatePropertyWidget(page, object.id, property);
        ++propertyIndex;
    }

    removeUnseenPropertyWidgets(page, object.id);
}

void QtRuntimeEditorWindow::updatePropertyWidget(RenderModePage& page, unsigned int objectId, const EditorUiProperty& property)
{
    RenderModePage::ControlObjectWidgets& objectWidgets = page.controlObjects[objectId];
    RenderModePage::PropertyWidgets&      propertyWidgets = objectWidgets.propertyWidgets[property.path];
    const int                             kind = static_cast<int>(property.kind);

    if (propertyWidgets.editor && propertyWidgets.kind != kind)
    {
        objectWidgets.form->removeRow(propertyWidgets.label);
        propertyWidgets = RenderModePage::PropertyWidgets();
    }

    if (!propertyWidgets.editor)
    {
        propertyWidgets.kind  = kind;
        propertyWidgets.label = new QLabel(toQString(property.label));
        propertyWidgets.label->setWordWrap(true);

        if (property.kind == EditorUiPropertyKind::Bool)
        {
            QCheckBox* check = new QCheckBox();
            propertyWidgets.editor = check;
            QObject::connect(check, &QCheckBox::toggled, this, [this, objectId, path = property.path](bool checked)
                             { requestSetObjectProperty(objectId, path, checked ? "true" : "false"); });
        }
        else if (property.kind == EditorUiPropertyKind::Number)
        {
            QDoubleSpinBox* spin = new QDoubleSpinBox();
            spin->setDecimals(3);
            spin->setKeyboardTracking(false);
            spin->setMinimumWidth(64);
            propertyWidgets.editor = spin;
            QObject::connect(spin, &QDoubleSpinBox::valueChanged, this, [this, objectId, path = property.path](double value)
                             { requestSetObjectProperty(objectId, path, toStdString(makeDoubleText(value))); });
        }
        else
        {
            QLineEdit* edit = new QLineEdit();
            propertyWidgets.editor = edit;
            QObject::connect(edit, &QLineEdit::editingFinished, this, [this, edit, objectId, path = property.path]()
                             { requestSetObjectProperty(objectId, path, toStdString(edit->text())); });
        }

        objectWidgets.form->addRow(propertyWidgets.label, propertyWidgets.editor);
    }

    propertyWidgets.seen = true;
    propertyWidgets.label->setText(toQString(property.label));

    if (property.kind == EditorUiPropertyKind::Bool)
    {
        QCheckBox* check = static_cast<QCheckBox*>(propertyWidgets.editor);
        check->setEnabled(property.editable);
        const QSignalBlocker blocker(check);
        check->setChecked(property.boolValue);
    }
    else if (property.kind == EditorUiPropertyKind::Number)
    {
        QDoubleSpinBox* spin = static_cast<QDoubleSpinBox*>(propertyWidgets.editor);
        spin->setEnabled(property.editable);
        const QSignalBlocker blocker(spin);
        applySpinBoxRange(spin, property);
        if (!widgetContainsFocus(spin))
        {
            spin->setValue(textToDouble(property.value));
        }
    }
    else
    {
        QLineEdit* edit = static_cast<QLineEdit*>(propertyWidgets.editor);
        edit->setReadOnly(!property.editable);
        if (!widgetContainsFocus(edit))
        {
            const QSignalBlocker blocker(edit);
            edit->setText(toQString(property.value));
        }
    }
}

void QtRuntimeEditorWindow::updateVectorPropertyWidget(RenderModePage& page, unsigned int objectId, const EditorUiObject& object,
                                                       size_t firstPropertyIndex, size_t propertyCount, const std::string& rootPath,
                                                       const std::string& label)
{
    RenderModePage::ControlObjectWidgets& objectWidgets = page.controlObjects[objectId];
    RenderModePage::PropertyWidgets&      propertyWidgets = objectWidgets.propertyWidgets[rootPath];
    const int                             kind = static_cast<int>(EditorUiPropertyKind::Number);

    bool needsRebuild = !propertyWidgets.editor || propertyWidgets.kind != kind || propertyWidgets.componentCount != static_cast<int>(propertyCount);
    if (!needsRebuild)
    {
        for (size_t componentOffset = 0; componentOffset < propertyCount; ++componentOffset)
        {
            if (propertyWidgets.componentPaths[componentOffset] != object.properties[firstPropertyIndex + componentOffset].path)
            {
                needsRebuild = true;
                break;
            }
        }
    }

    if (needsRebuild && propertyWidgets.editor)
    {
        objectWidgets.form->removeRow(propertyWidgets.label);
        propertyWidgets = RenderModePage::PropertyWidgets();
    }

    if (!propertyWidgets.editor)
    {
        propertyWidgets.kind           = kind;
        propertyWidgets.componentCount = static_cast<int>(propertyCount);
        propertyWidgets.label          = new QLabel(toQString(label));
        propertyWidgets.label->setWordWrap(true);

        QWidget*     row       = new QWidget();
        row->setMinimumWidth(0);
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        for (size_t componentOffset = 0; componentOffset < propertyCount && componentOffset < 4; ++componentOffset)
        {
            const EditorUiProperty& property = object.properties[firstPropertyIndex + componentOffset];
            QDoubleSpinBox*         spin     = new QDoubleSpinBox(row);
            spin->setDecimals(3);
            spin->setKeyboardTracking(false);
            spin->setMinimumWidth(56);
            spin->setToolTip(toQString(property.label));
            rowLayout->addWidget(spin);

            propertyWidgets.componentEditors[componentOffset] = spin;
            propertyWidgets.componentPaths[componentOffset]   = property.path;
            QObject::connect(spin, &QDoubleSpinBox::valueChanged, this, [this, objectId, path = property.path](double value)
                             { requestSetObjectProperty(objectId, path, toStdString(makeDoubleText(value))); });
        }

        propertyWidgets.editor = row;
        objectWidgets.form->addRow(propertyWidgets.label, propertyWidgets.editor);
    }

    propertyWidgets.seen = true;
    propertyWidgets.label->setText(toQString(label));

    for (size_t componentOffset = 0; componentOffset < propertyCount && componentOffset < 4; ++componentOffset)
    {
        const EditorUiProperty& property = object.properties[firstPropertyIndex + componentOffset];
        QDoubleSpinBox*         spin     = propertyWidgets.componentEditors[componentOffset];
        if (!spin)
        {
            continue;
        }

        spin->setEnabled(property.editable);
        const QSignalBlocker blocker(spin);
        applySpinBoxRange(spin, property);
        if (!widgetContainsFocus(spin))
        {
            spin->setValue(textToDouble(property.value));
        }
    }
}

void QtRuntimeEditorWindow::removeUnseenControlObjects(RenderModePage& page)
{
    for (auto objectIt = page.controlObjects.begin(); objectIt != page.controlObjects.end();)
    {
        RenderModePage::ControlObjectWidgets& widgets = objectIt->second;
        if (widgets.seen)
        {
            ++objectIt;
            continue;
        }

        page.controlsGrid->removeWidget(widgets.group);
        delete widgets.group;
        objectIt = page.controlObjects.erase(objectIt);
    }
}

void QtRuntimeEditorWindow::removeUnseenPropertyWidgets(RenderModePage& page, unsigned int objectId)
{
    RenderModePage::ControlObjectWidgets& objectWidgets = page.controlObjects[objectId];
    for (auto propertyIt = objectWidgets.propertyWidgets.begin(); propertyIt != objectWidgets.propertyWidgets.end();)
    {
        RenderModePage::PropertyWidgets& widgets = propertyIt->second;
        if (widgets.seen)
        {
            ++propertyIt;
            continue;
        }

        objectWidgets.form->removeRow(widgets.label);
        propertyIt = objectWidgets.propertyWidgets.erase(propertyIt);
    }
}

void QtRuntimeEditorWindow::requestCreateSceneNode(const std::string& renderModeId, const char* nodeType)
{
    const std::string parentNode = _selectedNodeByMode[renderModeId];
    const std::string nodeKey    = _editor.getRenderModeTabs().createSceneNode(renderModeId.c_str(), parentNode.c_str(), nodeType);
    if (!nodeKey.empty())
    {
        _selectedNodeByMode[renderModeId] = nodeKey;
        if (!parentNode.empty())
        {
            _expandedNodesByMode[renderModeId].insert(parentNode);
        }
    }
    scheduleRefresh();
}

void QtRuntimeEditorWindow::requestSetSceneNodeTransform(const std::string& renderModeId, const std::string& nodeKey, const char* path, double value)
{
    const std::string valueText = toStdString(makeDoubleText(value));
    _editor.getRenderModeTabs().setSceneNodeTransform(renderModeId.c_str(), nodeKey.c_str(), path, valueText.c_str());
    scheduleRefresh();
}

void QtRuntimeEditorWindow::requestAddSceneNodeComponent(const std::string& renderModeId, const std::string& nodeKey,
                                                         const std::string& componentType)
{
    _editor.getRenderModeTabs().addSceneNodeComponent(renderModeId.c_str(), nodeKey.c_str(), componentType.c_str());
    scheduleRefresh();
}

void QtRuntimeEditorWindow::requestLoadSceneNodeModel(const std::string& renderModeId, const std::string& nodeKey, const std::string& path)
{
    _editor.getRenderModeTabs().loadSceneNodeModel(renderModeId.c_str(), nodeKey.c_str(), path.c_str());
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
    if (_refreshScheduled)
    {
        return;
    }

    _refreshScheduled = true;
    QTimer::singleShot(0, this,
                       [this]()
                       {
                           if (_refreshScheduled)
                           {
                               refreshFromEditor();
                           }
                       });
}

} // namespace Play::editor
