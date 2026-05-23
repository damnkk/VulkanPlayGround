#include "editor/MainBar.h"

#include "editor/EditorRuntimeContext.h"

namespace Play::editor
{

MainBar::MainBar(EditorRuntimeContext& context) : _context(context)
{
}

bool MainBar::requestNewProject()
{
    return _context.requestNewProject();
}

bool MainBar::requestOpenProject()
{
    return _context.requestOpenProject();
}

bool MainBar::requestSaveProject()
{
    return _context.requestSaveProject();
}

void MainBar::appendHtml(std::string& html) const
{
    html += R"html(
    <header class="main-bar">
      <div class="editor-title">Editor</div>
      <nav class="menu-strip">
        <button class="menu-item" data-editor-command="new-project">New</button>
        <button class="menu-item" data-editor-command="open-project">Open</button>
        <button class="menu-item" data-editor-command="save-project">Save</button>
        <button class="menu-item" data-editor-command="settings">Settings</button>
      </nav>
    </header>
)html";
}

} // namespace Play::editor
