#include "EngineLoop.h"

#include "VulkanRuntime.h"

namespace Play::runtime
{

int EngineLoop::run(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo)
{
    VulkanRuntime runtime(config, contextInfo);
    if (!runtime.isInitialized())
    {
        return 1;
    }

    runtime.run();
    return 0;
}

} // namespace Play::runtime
