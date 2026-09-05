#ifndef PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
#define PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H

#include <QMainWindow>

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
    void startProject(const QString& projectPath);
    void createEditor();
    void refresh();

    RuntimeEditor&        _editor;
    SceneTreeWidget*      _sceneTree       = nullptr;
    InspectorWidget*      _inspector       = nullptr;
    AssetBrowserWidget*   _assetBrowser    = nullptr;
    QWidget*              _renderContainer = nullptr;
    ProjectStartupWidget* _startupPage     = nullptr;
    qulonglong            _revision        = 0;
    bool                  _hasSnapshot     = false;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
