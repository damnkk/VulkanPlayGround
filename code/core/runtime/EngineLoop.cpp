#include "EngineLoop.h"

#include "RuntimeGuiHost.h"
#include "VulkanRuntime.h"
#include "core/ProjectPaths.h"

namespace Play::runtime
{

int EngineLoop::run(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo)
{
    ProjectInfo::clear();
    RuntimeGuiHost guiHost;
    if (!guiHost.start())
    {
        return 1;
    }

    RuntimeConfig runtimeConfig = config;
    while (!guiHost.getEditor().takeStartupProject(runtimeConfig.projectPath, runtimeConfig.createNewProject))
    {
        if (guiHost.getEditor().exitRequested())
        {
            guiHost.stop();
            return 0;
        }
        SDL_Delay(16);
    }

    // Both new and existing projects have a save directory before runtime initialization.
    if (!ProjectInfo::setProjectPath(std::filesystem::u8path(runtimeConfig.projectPath)))
    {
        guiHost.stop();
        return 1;
    }

    int result = 0;
    {
        VulkanRuntime runtime(runtimeConfig, contextInfo, guiHost);
        if (!runtime.isInitialized())
        {
            result = 1;
        }
        else
        {
            runtime.run();
        }

        guiHost.stop();
    }

    return result;
}

} // namespace Play::runtime
