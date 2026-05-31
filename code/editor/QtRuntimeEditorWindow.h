#ifndef PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
#define PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H

#include <QMainWindow>

#include <map>
#include <set>
#include <string>

class QTabWidget;
class QTimer;

namespace Play::editor
{

class RuntimeEditor;
struct EditorUiRenderMode;
struct EditorUiSceneNode;

class QtRuntimeEditorWindow final : public QMainWindow
{
public:
    explicit QtRuntimeEditorWindow(RuntimeEditor& editor, QWidget* parent = nullptr);

    void refreshFromEditor();

private:
    QWidget* buildRenderModePage(const EditorUiRenderMode& renderMode);
    QWidget* buildScenePanel(const EditorUiRenderMode& renderMode);
    QWidget* buildInspectorPanel(const EditorUiRenderMode& renderMode);
    QWidget* buildControlsPanel(const EditorUiRenderMode& renderMode);

    void requestCreateSceneNode(const std::string& renderModeId, const char* nodeType);
    void requestSetSceneNodeTransform(const std::string& renderModeId, const std::string& nodeKey, const char* path, double value);
    void requestAddSceneNodeComponent(const std::string& renderModeId, const std::string& nodeKey);
    void requestSetObjectProperty(unsigned int objectId, const std::string& propertyPath, const std::string& value);
    void requestResetObject(unsigned int objectId);
    void scheduleRefresh();

    RuntimeEditor&                               _editor;
    QTabWidget*                                  _tabs         = nullptr;
    QTimer*                                      _refreshTimer = nullptr;
    bool                                         _refreshing   = false;
    std::string                                  _currentRenderMode;
    std::map<std::string, std::string>           _selectedNodeByMode;
    std::map<std::string, std::set<std::string>> _expandedNodesByMode;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
