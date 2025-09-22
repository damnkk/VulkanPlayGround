#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "Nsight/aftermath/include/GFSDK_Aftermath_Defines.h"
#include "Nsight/aftermath/include/GFSDK_Aftermath_GpuCrashDump.h"
#include <string>
#include <memory>

namespace Play
{
struct Debugger
{
};

// Automated Crash Reporter using NVIDIA Aftermath
class AftermathCrashReporter
{
public:
    static bool initialize();
    static void shutdown();
    static bool checkDeviceLost();
    static void waitForCrashDumpCollection();
    
private:
    static void GFSDK_AFTERMATH_CALL onCrashDump(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData);
    static void GFSDK_AFTERMATH_CALL onShaderDebugInfo(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize, void* pUserData);
    static void GFSDK_AFTERMATH_CALL onDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addValue, void* pUserData);
    
    static void saveCrashDumpToFile(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize);
    static void saveShaderDebugInfoToFile(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize);
    
    static bool s_initialized;
    static uint32_t s_crashDumpCounter;
    static uint32_t s_shaderDebugInfoCounter;
};

struct NsightDebugger : public Debugger
{
    static bool initInjection();
    static void capture();
};
}

#endif // DEBUGGER_H