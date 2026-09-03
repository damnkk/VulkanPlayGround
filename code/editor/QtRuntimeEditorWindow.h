#ifndef PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
#define PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H

#include <QMainWindow>

namespace Play::editor
{

class RuntimeEditor;

class QtRuntimeEditorWindow final : public QMainWindow
{
public:
    explicit QtRuntimeEditorWindow(RuntimeEditor& editor, QWidget* parent = nullptr);
    ~QtRuntimeEditorWindow() override;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_QTRUNTIMEEDITORWINDOW_H
