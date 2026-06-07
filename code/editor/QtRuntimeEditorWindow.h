#ifndef PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
#define PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H

#include <QMainWindow>

#include <map>
#include <set>
#include <string>

class QTabWidget;
class QTimer;
class QTreeWidgetItem;

namespace Play::editor
{

class RuntimeEditor;
struct EditorUiObject;
struct EditorUiProperty;
struct EditorUiRenderMode;
struct EditorUiSnapshot;
struct EditorUiSceneNode;

class QtRuntimeEditorWindow final : public QMainWindow
{
public:
    explicit QtRuntimeEditorWindow(RuntimeEditor& editor, QWidget* parent = nullptr);
    ~QtRuntimeEditorWindow() override;

    void refreshFromEditor();

private:
    struct RenderModePage;

    RenderModePage* createRenderModePage(const EditorUiRenderMode& renderMode);
    void            updateRenderModeTabs(const EditorUiSnapshot& snapshot);
    void            updateRenderModePage(RenderModePage& page, const EditorUiRenderMode& renderMode);
    void            updateScenePanel(RenderModePage& page, const EditorUiRenderMode& renderMode);
    void            updateInspectorPanel(RenderModePage& page, const EditorUiRenderMode& renderMode);
    void            updateControlsPanel(RenderModePage& page, const EditorUiRenderMode& renderMode);
    void            updateControlObject(RenderModePage& page, const EditorUiObject& object);
    void            updatePropertyWidget(RenderModePage& page, unsigned int objectId, const EditorUiProperty& property);
    void            updateVectorPropertyWidget(RenderModePage& page, unsigned int objectId, const EditorUiObject& object, size_t firstPropertyIndex,
                                               size_t propertyCount, const std::string& rootPath, const std::string& label);
    void            removeUnseenControlObjects(RenderModePage& page);
    void            removeUnseenPropertyWidgets(RenderModePage& page, unsigned int objectId);

    QTreeWidgetItem* syncSceneTreeNode(RenderModePage& page, const EditorUiSceneNode& node, QTreeWidgetItem* parent, int row);
    void             removeSceneTreeItem(RenderModePage& page, QTreeWidgetItem* item);
    void             clearSceneTree(RenderModePage& page);

    void requestCreateSceneNode(const std::string& renderModeId, const char* nodeType);
    void requestSetSceneNodeTransform(const std::string& renderModeId, const std::string& nodeKey, const char* path, double value);
    void requestAddSceneNodeComponent(const std::string& renderModeId, const std::string& nodeKey, const std::string& componentType);
    void requestLoadSceneNodeModel(const std::string& renderModeId, const std::string& nodeKey, const std::string& path);
    void requestSetObjectProperty(unsigned int objectId, const std::string& propertyPath, const std::string& value);
    void requestResetObject(unsigned int objectId);
    void scheduleRefresh();

    RuntimeEditor&                               _editor;
    QTabWidget*                                  _tabs         = nullptr;
    QTimer*                                      _refreshTimer = nullptr;
    bool                                         _refreshing   = false;
    bool                                         _refreshScheduled = false;
    std::string                                  _currentRenderMode;
    std::map<std::string, std::string>           _selectedNodeByMode;
    std::map<std::string, std::set<std::string>> _expandedNodesByMode;
    std::map<std::string, RenderModePage*>       _pagesByMode;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
