#include "EngineLoop.h"

#include "RuntimeGuiHost.h"
#include "VulkanRuntime.h"
#include "core/ProjectPaths.h"

namespace Play::runtime
{

int EngineLoop::run(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo)
{
    RuntimeGuiHost guiHost;
    if (!guiHost.start())
    {
        return 1;
    }

    RuntimeConfig runtimeConfig = config;
    if (runtimeConfig.projectPath.empty())
    {
        runtimeConfig.projectPath = getBaseFilePath().string();
    }

    if (!ProjectInfo::setProjectPath(runtimeConfig.projectPath))
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
