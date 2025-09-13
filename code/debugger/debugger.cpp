#include "debugger.h"
#include "pch.h"
#include <windows.h>
#include <debugapi.h>
#include "nvnsight/nsightevents.hpp"
#include "nvaftermath/aftermath.hpp"
#include "nvvk/check_error.hpp"
namespace Play
{

std::vector<nvvk::ExtensionInfo> NsightDebugger::initInjection()
{
    auto& afterMathTracker = AftermathCrashTracker::getInstance();
    afterMathTracker.initialize();
    std::vector<nvvk::ExtensionInfo> extensions;
    afterMathTracker.addExtensions(extensions);
    nvvk::CheckError::getInstance().setCallbackFunction(
        [&](VkResult result) { afterMathTracker.errorCallback(result); });
    return extensions;
}

} // namespace Play