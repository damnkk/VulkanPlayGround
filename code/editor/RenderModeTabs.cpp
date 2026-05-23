#include "editor/RenderModeTabs.h"

#include "core/runtime/RenderSession.h"
#include "editor/EditorHtml.h"
#include "editor/EditorRuntimeContext.h"
#include "editor/RenderModeEditor.h"

namespace Play::editor
{

namespace
{
bool isSameText(const char* lhs, const char* rhs)
{
    return std::string(lhs ? lhs : "") == std::string(rhs ? rhs : "");
}
} // namespace

struct RenderModeTabs::Impl
{
    std::vector<std::unique_ptr<RenderModeEditor>> editors;
    std::string                                    activeMode;
};

RenderModeTabs::RenderModeTabs(EditorRuntimeContext& context, EditorRegistry& editorRegistry)
    : _context(context), _editorRegistry(editorRegistry), _impl(new Impl())
{
}

RenderModeTabs::~RenderModeTabs()
{
    delete _impl;
    _impl = nullptr;
}

RenderModeEditor& RenderModeTabs::addRenderMode(const char* id, const char* title, EditorRenderMode renderMode)
{
    _impl->editors.push_back(std::unique_ptr<RenderModeEditor>(new RenderModeEditor(id, title, _editorRegistry, renderMode)));
    if (_impl->activeMode.empty())
    {
        _impl->activeMode = id ? id : "";
    }
    return *_impl->editors.back();
}

RenderModeEditor* RenderModeTabs::findRenderMode(const char* id)
{
    for (const std::unique_ptr<RenderModeEditor>& editor : _impl->editors)
    {
        if (isSameText(editor->getId(), id))
        {
            return editor.get();
        }
    }

    return nullptr;
}

void RenderModeTabs::bindRenderSession(Play::RenderSession& renderSession, const char* activeMode)
{
    _impl->activeMode = activeMode ? activeMode : "";
    Play::SceneManager* sceneManager = renderSession.getSceneManager();
    for (const std::unique_ptr<RenderModeEditor>& editor : _impl->editors)
    {
        editor->setSceneManager(isSameText(editor->getId(), activeMode) ? sceneManager : nullptr);
    }
}

bool RenderModeTabs::requestActiveMode(const char* id)
{
    _impl->activeMode = id ? id : "";
    return _context.requestRenderMode(id);
}

void RenderModeTabs::appendHtml(std::string& html) const
{
    html += "<section class=\"render-mode-tabs\"><nav class=\"tab-strip\">";
    for (const std::unique_ptr<RenderModeEditor>& editor : _impl->editors)
    {
        editor->appendTabHtml(html, isSameText(editor->getId(), _impl->activeMode.c_str()));
    }
    html += "</nav><main class=\"render-mode-pages\">";
    for (const std::unique_ptr<RenderModeEditor>& editor : _impl->editors)
    {
        editor->appendPageHtml(html, isSameText(editor->getId(), _impl->activeMode.c_str()));
    }
    html += "</main></section>";
}

} // namespace Play::editor
