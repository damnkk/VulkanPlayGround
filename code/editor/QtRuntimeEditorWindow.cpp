#include "editor/QtRuntimeEditorWindow.h"

#include <QCloseEvent>
#include <QDockWidget>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include <QWindow>

#include "editor/AssetBrowserWidget.h"
#include "editor/InspectorWidget.h"
#include "editor/RuntimeEditor.h"
#include "editor/SceneTreeWidget.h"

namespace Play::editor
{

QtRuntimeEditorWindow::QtRuntimeEditorWindow(RuntimeEditor& editor, QWidget* parent) : QMainWindow(parent), _editor(editor)
{
    setWindowTitle("VulkanPlayGround Editor");
    resize(1100, 720);

    QLabel* placeholder = new QLabel("The Vulkan viewport is running in the SDL window.\nThis window only owns editor controls.", this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);

    _sceneTree = new SceneTreeWidget(_editor, this);
    QDockWidget* sceneDock = new QDockWidget("Scene", this);
    sceneDock->setObjectName("SceneDock");
    sceneDock->setWidget(_sceneTree);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    _inspector = new InspectorWidget(_editor, this);
    QDockWidget* inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setObjectName("InspectorDock");
    inspectorDock->setWidget(_inspector);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    _assetBrowser = new AssetBrowserWidget(this);
    QDockWidget* assetDock = new QDockWidget("Assets", this);
    assetDock->setObjectName("AssetDock");
    assetDock->setWidget(_assetBrowser);
    addDockWidget(Qt::BottomDockWidgetArea, assetDock);

    connect(_sceneTree, &SceneTreeWidget::nodeSelected, _inspector,
            [this](qulonglong nodeId) { _inspector->selectNode(static_cast<EditorNodeId>(nodeId)); });

    statusBar()->showMessage("Waiting for an engine snapshot");
    QTimer* refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &QtRuntimeEditorWindow::refresh);
    refreshTimer->start(200);
    refresh();
}

QtRuntimeEditorWindow::~QtRuntimeEditorWindow() = default;

void QtRuntimeEditorWindow::closeEvent(QCloseEvent* event)
{
    _editor.requestExit();
    event->accept();
}

void QtRuntimeEditorWindow::setRenderWindowHandle(void* handle)
{
    if (_renderContainer || !handle)
    {
        return;
    }

    QWindow* renderWindow = QWindow::fromWinId(reinterpret_cast<WId>(handle));
    _renderContainer      = QWidget::createWindowContainer(renderWindow, this);
    _renderContainer->setFocusPolicy(Qt::StrongFocus);
    setCentralWidget(_renderContainer);
}

void QtRuntimeEditorWindow::refresh()
{
    const EditorSnapshot snapshot = _editor.getSnapshot();
    if (_hasSnapshot && snapshot.revision == _revision)
    {
        return;
    }

    _revision = snapshot.revision;
    _hasSnapshot = true;
    _sceneTree->refresh(snapshot);
    _inspector->setSnapshot(snapshot);
    _assetBrowser->refresh(snapshot);
    statusBar()->showMessage(snapshot.sceneName.empty() ? "Waiting for an engine snapshot" : QString::fromStdString(snapshot.sceneName));
}

} // namespace Play::editor
