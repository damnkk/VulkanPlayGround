#pragma once

namespace Play
{
class RenderSession;
}

namespace Play::runtime
{
class VulkanRuntime;
}

namespace Play::editor
{

class EditorRegistry;

class EditorRuntimeContext
{
public:
    void bind(Play::runtime::VulkanRuntime& runtime, Play::RenderSession& renderSession, EditorRegistry& editorRegistry);

    Play::runtime::VulkanRuntime* getRuntime() const
    {
        return _runtime;
    }

    Play::RenderSession* getRenderSession() const
    {
        return _renderSession;
    }

    EditorRegistry* getEditorRegistry() const
    {
        return _editorRegistry;
    }

    bool requestRenderMode(const char* renderMode);
    bool requestNewProject();
    bool requestOpenProject();
    bool requestSaveProject();

private:
    Play::runtime::VulkanRuntime* _runtime        = nullptr;
    Play::RenderSession*          _renderSession  = nullptr;
    EditorRegistry*               _editorRegistry = nullptr;
};

} // namespace Play::editor
