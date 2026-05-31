#ifndef PLAY_CODE_EDITOR_RENDERMODETABS_H
#define PLAY_CODE_EDITOR_RENDERMODETABS_H

#include "editor/EditorRegistry.h"

namespace Play
{
class RenderSession;
}

namespace Play::editor
{

class EditorRuntimeContext;
class RenderModeEditor;

class RenderModeTabs
{
public:
    RenderModeTabs(EditorRuntimeContext& context, EditorRegistry& editorRegistry);
    ~RenderModeTabs();

    RenderModeTabs(const RenderModeTabs&)            = delete;
    RenderModeTabs& operator=(const RenderModeTabs&) = delete;
    RenderModeTabs(RenderModeTabs&&)                 = delete;
    RenderModeTabs& operator=(RenderModeTabs&&)      = delete;

    RenderModeEditor& addRenderMode(const char* id, const char* title, EditorRenderMode renderMode);
    RenderModeEditor* findRenderMode(const char* id);
    void              bindRenderSession(Play::RenderSession& renderSession, const char* activeMode);
    bool              requestActiveMode(const char* id);
    std::string       createSceneNode(const char* renderModeId, const char* parentNodeKey, const char* nodeType);
    bool              setSceneNodeTransform(const char* renderModeId, const char* nodeKey, const char* transformPath, const char* value);
    bool              addSceneNodeComponent(const char* renderModeId, const char* nodeKey, const char* componentType);
    void              buildSnapshot(EditorUiSnapshot& snapshot) const;

private:
    struct Impl;

    EditorRuntimeContext& _context;
    EditorRegistry&       _editorRegistry;
    Impl*                 _impl = nullptr;
};

} // namespace Play::editor

#endif // PLAY_CODE_EDITOR_RENDERMODETABS_H
