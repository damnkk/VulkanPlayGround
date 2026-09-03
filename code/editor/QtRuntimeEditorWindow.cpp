#include "editor/QtRuntimeEditorWindow.h"

#include <QLabel>

namespace Play::editor
{

QtRuntimeEditorWindow::QtRuntimeEditorWindow(RuntimeEditor&, QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("VulkanPlayGround Editor");
    resize(640, 360);

    QLabel* placeholder = new QLabel("Editor UI is being rebuilt.", this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);
}

QtRuntimeEditorWindow::~QtRuntimeEditorWindow() = default;

} // namespace Play::editor
