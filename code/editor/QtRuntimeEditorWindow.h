#ifndef PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
#define PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H

#include <QMainWindow>

#include "editor/EditorProtocol.h"

class QCloseEvent;

namespace Play::editor
{

class AssetBrowserWidget;
class InspectorWidget;
class ProjectStartupWidget;
class RuntimeEditor;
class SceneTreeWidget;

class QtRuntimeEditorWindow final : public QMainWindow
{
public:
    explicit QtRuntimeEditorWindow(RuntimeEditor& editor, QWidget* parent = nullptr);
    ~QtRuntimeEditorWindow() override;

    void setRenderWindowHandle(void* handle);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void showStartupPage();
    void startProject(const QString& projectPath, bool createNew);
    void createEditor();
    void createEditorActions();
    void requestSave();
    void refresh();

    RuntimeEditor&        _editor;
    EditorSnapshot        _snapshot;
    SceneTreeWidget*      _sceneTree       = nullptr;
    InspectorWidget*      _inspector       = nullptr;
    AssetBrowserWidget*   _assetBrowser    = nullptr;
    QWidget*              _renderContainer = nullptr;
    ProjectStartupWidget* _startupPage     = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
