#include "EngineLoop.h"

#include "VulkanRuntime.h"

namespace Play::runtime
{

int EngineLoop::run(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo)
{
    VulkanRuntime runtime;
    if (!runtime.init(config, contextInfo))
    {
        return 1;
    }

    runtime.run();
    runtime.deinit();
    return 0;
}

} // namespace Play::runtime
