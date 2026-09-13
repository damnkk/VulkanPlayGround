#include "editor/QtRuntimeEditorWindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDir>
#include <QLabel>
#include <QMenu>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
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

void QtRuntimeEditorWindow::startProject(const QString& projectPath, bool createNew)
{
    StartupProjectReceiver* receiver = _editor.getStartupProjectReceiver();
    std::string             errorMessage;
    if (!receiver || !receiver->selectStartupProject(projectPath.toStdString(), createNew, &errorMessage))
    {
        _startupPage->showError(errorMessage.empty() ? "Project startup is not available." : QString::fromStdString(errorMessage));
        return;
    }

    _startupPage->showStarting();
    setWindowTitle(tr("%1 - VulkanPlayGround").arg(QDir(projectPath).dirName()));
    auto* location = new QLabel(tr("Save location: %1").arg(QDir::toNativeSeparators(projectPath)), this);
    location->setToolTip(QDir::toNativeSeparators(projectPath));
    location->setTextFormat(Qt::PlainText);
    location->setTextInteractionFlags(Qt::TextSelectableByMouse);
    location->setMinimumWidth(0);
    location->setMaximumWidth(580);
    statusBar()->addPermanentWidget(location);
    statusBar()->hide();
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

    _assetBrowser          = new AssetBrowserWidget(_editor, this);
    QDockWidget* assetDock = new QDockWidget("Assets", this);
    assetDock->setObjectName("AssetDock");
    assetDock->setWidget(_assetBrowser);
    addDockWidget(Qt::BottomDockWidgetArea, assetDock);
    createEditorActions();

    connect(_sceneTree, &SceneTreeWidget::nodeSelected, _inspector,
            [this](qulonglong nodeId) { _inspector->selectNode(static_cast<EditorNodeId>(nodeId)); });

    statusBar()->show();
    statusBar()->showMessage("Waiting for an engine snapshot");
    QTimer* refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &QtRuntimeEditorWindow::refresh);
    refreshTimer->start(200);
    refresh();
}

void QtRuntimeEditorWindow::createEditorActions()
{
    QAction* saveAction = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton), tr("&Save"), this);
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setShortcutContext(Qt::WindowShortcut);
    saveAction->setAutoRepeat(false);
    saveAction->setToolTip(tr("Save the current project (Ctrl+S)"));
    connect(saveAction, &QAction::triggered, this, &QtRuntimeEditorWindow::requestSave);

    QAction* importAction = new QAction(style()->standardIcon(QStyle::SP_DialogOpenButton), tr("&Import Asset..."), this);
    importAction->setToolTip(tr("Import external resources into the project"));
    connect(importAction, &QAction::triggered, _assetBrowser, &AssetBrowserWidget::importSelectedAssetType);

    QToolBar* editorBar = addToolBar(tr("Editor"));
    editorBar->setObjectName("EditorToolBar");
    editorBar->setMovable(false);
    editorBar->setFloatable(false);
    editorBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    QToolButton* fileButton = new QToolButton(editorBar);
    fileButton->setText(tr("&File"));
    fileButton->setToolTip(tr("Project and asset operations"));
    fileButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    fileButton->setPopupMode(QToolButton::InstantPopup);
    fileButton->setAutoRaise(true);
    QMenu* fileMenu = new QMenu(fileButton);
    fileMenu->addAction(saveAction);
    fileMenu->addSeparator();
    fileMenu->addAction(importAction);
    fileButton->setMenu(fileMenu);
    editorBar->addWidget(fileButton);
}

void QtRuntimeEditorWindow::requestSave()
{
    _editor.requestSave();
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
