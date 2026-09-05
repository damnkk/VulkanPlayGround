#include "editor/QtRuntimeEditorWindow.h"

#include <QCloseEvent>
#include <QDockWidget>
#include <QStatusBar>
#include <QTimer>
#include <QWindow>

#include "editor/AssetBrowserWidget.h"
#include "editor/InspectorWidget.h"
#include "editor/ProjectStartupWidget.h"
#include "editor/RuntimeEditor.h"
#include "editor/SceneTreeWidget.h"

namespace Play::editor
{

QtRuntimeEditorWindow::QtRuntimeEditorWindow(RuntimeEditor& editor, QWidget* parent) : QMainWindow(parent), _editor(editor)
{
    setWindowTitle("VulkanPlayGround Editor");
    resize(1120, 800);
    showStartupPage();
}

void QtRuntimeEditorWindow::showStartupPage()
{
    _startupPage = new ProjectStartupWidget(this);
    setCentralWidget(_startupPage);
    connect(_startupPage, &ProjectStartupWidget::projectSelected, this, &QtRuntimeEditorWindow::startProject);
}

void QtRuntimeEditorWindow::startProject(const QString& projectPath)
{
    StartupProjectReceiver* receiver = _editor.getStartupProjectReceiver();
    std::string             errorMessage;
    if (!receiver || !receiver->selectStartupProject(projectPath.toStdString(), &errorMessage))
    {
        _startupPage->showError(errorMessage.empty() ? "Project startup is not available." : QString::fromStdString(errorMessage));
        return;
    }

    _startupPage->showStarting();
}

void QtRuntimeEditorWindow::createEditor()
{
    resize(1100, 720);

    _sceneTree             = new SceneTreeWidget(_editor, this);
    QDockWidget* sceneDock = new QDockWidget("Scene", this);
    sceneDock->setObjectName("SceneDock");
    sceneDock->setWidget(_sceneTree);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    _inspector                 = new InspectorWidget(_editor, this);
    QDockWidget* inspectorDock = new QDockWidget("Inspector", this);
    inspectorDock->setObjectName("InspectorDock");
    inspectorDock->setWidget(_inspector);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);

    _assetBrowser          = new AssetBrowserWidget(this);
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
    // Hide the editor and its native viewport together. The engine stops the
    // Qt event loop after rendering has finished, so the container stays alive.
    event->ignore();
    hide();
    _editor.requestExit();
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
    createEditor();
    setCentralWidget(_renderContainer);
    _startupPage = nullptr;
    refresh();
    if (!_editor.exitRequested())
    {
        show();
    }
}

void QtRuntimeEditorWindow::refresh()
{
    const EditorSnapshot snapshot = _editor.getSnapshot();
    if (_hasSnapshot && snapshot.revision == _revision)
    {
        return;
    }

    _revision    = snapshot.revision;
    _hasSnapshot = true;
    _sceneTree->refresh(snapshot);
    _inspector->setSnapshot(snapshot);
    _assetBrowser->refresh(snapshot);
    statusBar()->showMessage(snapshot.sceneName.empty() ? "Waiting for an engine snapshot" : QString::fromStdString(snapshot.sceneName));
}

} // namespace Play::editor
