#include "editor/RuntimeEditor.h"

#include "core/runtime/RenderSession.h"
#include "core/runtime/VulkanRuntime.h"
#include "resourceManagement/scene/SceneManager.h"

namespace Play::editor
{

RuntimeEditor::RuntimeEditor() : _renderModeTabs(_runtimeContext, _editorRegistry)
{
    _renderModeTabs.addRenderMode("defer", "Defer", EditorRenderMode::Defer);
    _renderModeTabs.addRenderMode("gaussian", "Gaussian", EditorRenderMode::Gaussian);
    _renderModeTabs.addRenderMode("raytrace", "Ray Tracing", EditorRenderMode::Raytrace);
    _renderModeTabs.addRenderMode("volume", "Volume", EditorRenderMode::Volume);
}

void RuntimeEditor::bindRuntime(Play::runtime::VulkanRuntime& runtime, Play::RenderSession& renderSession, const char* activeMode)
{
    _runtimeContext.bind(runtime, renderSession, _editorRegistry);
    _renderModeTabs.bindRenderSession(renderSession, activeMode);
}

void RuntimeEditor::setStartupProjectReceiver(StartupProjectReceiver* receiver)
{
    _startupProjectReceiver = receiver;
}

EditorUiSnapshot RuntimeEditor::buildSnapshot() const
{
    EditorUiSnapshot snapshot;
    _renderModeTabs.buildSnapshot(snapshot);
    return snapshot;
}

bool RuntimeEditor::createProject(const std::string& projectPath, std::string* errorMessage)
{
    if (!isRuntimeBound())
    {
        if (!_startupProjectReceiver)
        {
            if (errorMessage)
            {
                *errorMessage = "Project startup is not available.";
            }
            return false;
        }

        return _startupProjectReceiver->selectStartupProject(projectPath, errorMessage);
    }

    Play::SceneManager* sceneManager = Play::vkDriver ? Play::vkDriver->getSceneManager() : nullptr;
    if (!sceneManager)
    {
        if (errorMessage)
        {
            *errorMessage = "The render scene is not ready.";
        }
        return false;
    }

    return sceneManager->createProject(projectPath, errorMessage);
}

bool RuntimeEditor::saveProject(std::string* errorMessage)
{
    Play::SceneManager* sceneManager = Play::vkDriver ? Play::vkDriver->getSceneManager() : nullptr;
    if (!sceneManager)
    {
        if (errorMessage)
        {
            *errorMessage = "The render scene is not ready.";
        }
        return false;
    }

    return sceneManager->saveProject(errorMessage);
}

bool RuntimeEditor::loadProject(const std::string& projectPath, std::string* errorMessage)
{
    if (!isRuntimeBound())
    {
        if (!_startupProjectReceiver)
        {
            if (errorMessage)
            {
                *errorMessage = "Project startup is not available.";
            }
            return false;
        }

        return _startupProjectReceiver->selectStartupProject(projectPath, errorMessage);
    }

    Play::SceneManager* sceneManager = Play::vkDriver ? Play::vkDriver->getSceneManager() : nullptr;
    if (!sceneManager)
    {
        if (errorMessage)
        {
            *errorMessage = "The render scene is not ready.";
        }
        return false;
    }

    return sceneManager->loadProject(projectPath, errorMessage);
}

} // namespace Play::editor
