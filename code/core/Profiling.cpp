#include "Profiling.h"

#if PLAY_ENABLE_PROFILING
#include <chrono>
#include <nvutils/logger.hpp>

#if PLAY_ENABLE_NVTX
#include <nvtx3/nvToolsExt.h>
#endif

namespace Play
{
namespace
{
const char* safeProfileName(const char* name)
{
    return name ? name : "<unnamed>";
}

uint64_t getCurrentTimeNs()
{
    using Clock = std::chrono::steady_clock;
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}

void pushNvtxRange(const char* name)
{
#if PLAY_ENABLE_NVTX
    nvtxRangePushA(safeProfileName(name));
#endif
}

void popNvtxRange()
{
#if PLAY_ENABLE_NVTX
    nvtxRangePop();
#endif
}

void markNvtxEvent(const char* name)
{
#if PLAY_ENABLE_NVTX
    nvtxMarkA(safeProfileName(name));
#endif
}
} // namespace

ScopedProfileZone::ScopedProfileZone(const char* name) : _name(name)
{
    pushNvtxRange(_name);
    _startNs = getCurrentTimeNs();
}

ScopedProfileZone::~ScopedProfileZone()
{
    const uint64_t endNs     = getCurrentTimeNs();
    const double   elapsedMs = static_cast<double>(endNs - _startNs) / 1000000.0;
    popNvtxRange();
#if PLAY_ENABLE_PROFILE_LOG
    LOGI("[Profile] %s: %.3f ms\n", safeProfileName(_name), elapsedMs);
#endif
}

ScopedCommandLabel::ScopedCommandLabel(VkCommandBuffer cmd, const char* name) : ScopedCommandLabel(cmd, name, 0.20f, 0.55f, 1.0f, 1.0f) {}

ScopedCommandLabel::ScopedCommandLabel(VkCommandBuffer cmd, const char* name, float red, float green, float blue, float alpha) : _cmd(cmd)
{
    if (_cmd == VK_NULL_HANDLE || vkCmdBeginDebugUtilsLabelEXT == nullptr)
    {
        return;
    }

    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = safeProfileName(name);
    label.color[0]   = red;
    label.color[1]   = green;
    label.color[2]   = blue;
    label.color[3]   = alpha;
    vkCmdBeginDebugUtilsLabelEXT(_cmd, &label);
    _active = true;
}

ScopedCommandLabel::~ScopedCommandLabel()
{
    if (_active && vkCmdEndDebugUtilsLabelEXT != nullptr)
    {
        vkCmdEndDebugUtilsLabelEXT(_cmd);
    }
}

void markProfileEvent(const char* name)
{
    markNvtxEvent(name);
}
} // namespace Play
#endif
