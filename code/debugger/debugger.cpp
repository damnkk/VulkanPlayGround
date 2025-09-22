#include "debugger.h"
#include "pch.h"
#include <windows.h>
#include <debugapi.h>
#include <fstream>
#include <chrono>
#include <iostream>
#include <filesystem>
#include "Nsight/graphics/include/NGFX_Injection.h"

// Forward declare Aftermath functions - they will be dynamically linked
extern "C" {
    // These functions will be resolved at runtime if Aftermath libraries are available
    typedef GFSDK_Aftermath_Result (GFSDK_AFTERMATH_CALL* PFN_GFSDK_Aftermath_EnableGpuCrashDumps)(
        GFSDK_Aftermath_Version apiVersion,
        uint32_t watchedApis,
        uint32_t flags,
        PFN_GFSDK_Aftermath_GpuCrashDumpCb gpuCrashDumpCb,
        PFN_GFSDK_Aftermath_ShaderDebugInfoCb shaderDebugInfoCb,
        PFN_GFSDK_Aftermath_GpuCrashDumpDescriptionCb descriptionCb,
        PFN_GFSDK_Aftermath_ResolveMarkerCb resolveMarkerCb,
        void* pUserData);
    
    typedef GFSDK_Aftermath_Result (GFSDK_AFTERMATH_CALL* PFN_GFSDK_Aftermath_DisableGpuCrashDumps)();
    typedef GFSDK_Aftermath_Result (GFSDK_AFTERMATH_CALL* PFN_GFSDK_Aftermath_GetCrashDumpStatus)(GFSDK_Aftermath_CrashDump_Status* pOutStatus);
}

namespace Play
{
// AftermathCrashReporter static members
bool AftermathCrashReporter::s_initialized = false;
uint32_t AftermathCrashReporter::s_crashDumpCounter = 0;
uint32_t AftermathCrashReporter::s_shaderDebugInfoCounter = 0;

// Function pointers for dynamic loading
static PFN_GFSDK_Aftermath_EnableGpuCrashDumps pfnEnableGpuCrashDumps = nullptr;
static PFN_GFSDK_Aftermath_DisableGpuCrashDumps pfnDisableGpuCrashDumps = nullptr;
static PFN_GFSDK_Aftermath_GetCrashDumpStatus pfnGetCrashDumpStatus = nullptr;
static HMODULE hAftermathModule = nullptr;

static bool LoadAftermathLibrary()
{
    if (hAftermathModule != nullptr)
    {
        return true;
    }

    // Try to load the Aftermath library
    hAftermathModule = LoadLibraryA("GFSDK_Aftermath_Lib.dll");
    if (hAftermathModule == nullptr)
    {
        std::wcout << L"Aftermath library not found. Crash reporting will be disabled." << std::endl;
        return false;
    }

    // Load function pointers
    pfnEnableGpuCrashDumps = (PFN_GFSDK_Aftermath_EnableGpuCrashDumps)GetProcAddress(hAftermathModule, "GFSDK_Aftermath_EnableGpuCrashDumps");
    pfnDisableGpuCrashDumps = (PFN_GFSDK_Aftermath_DisableGpuCrashDumps)GetProcAddress(hAftermathModule, "GFSDK_Aftermath_DisableGpuCrashDumps");
    pfnGetCrashDumpStatus = (PFN_GFSDK_Aftermath_GetCrashDumpStatus)GetProcAddress(hAftermathModule, "GFSDK_Aftermath_GetCrashDumpStatus");

    if (!pfnEnableGpuCrashDumps || !pfnDisableGpuCrashDumps || !pfnGetCrashDumpStatus)
    {
        std::wcout << L"Failed to load Aftermath function pointers. Crash reporting will be disabled." << std::endl;
        FreeLibrary(hAftermathModule);
        hAftermathModule = nullptr;
        return false;
    }

    std::wcout << L"Aftermath library loaded successfully." << std::endl;
    return true;
}

bool AftermathCrashReporter::initialize()
{
    if (s_initialized)
    {
        return true;
    }

    // Try to load Aftermath library
    if (!LoadAftermathLibrary())
    {
        std::wcout << L"Aftermath crash reporting initialization skipped - library not available." << std::endl;
        return false;
    }

    // Create crash dump directory if it doesn't exist
    std::filesystem::create_directories("crash_dumps");

    // Enable GPU crash dumps for Vulkan API
    const GFSDK_Aftermath_Result result = pfnEnableGpuCrashDumps(
        GFSDK_Aftermath_Version_API,
        GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_Vulkan,
        GFSDK_Aftermath_GpuCrashDumpFeatureFlags_DeferDebugInfoCallbacks,
        onCrashDump,
        onShaderDebugInfo,
        onDescription,
        nullptr, // resolveMarkerCb
        nullptr  // pUserData
    );

    if (result != GFSDK_Aftermath_Result_Success)
    {
        std::wcout << L"Failed to initialize Aftermath GPU crash dumps. Error: " << result << std::endl;
        return false;
    }

    s_initialized = true;
    std::wcout << L"Aftermath GPU crash dump monitoring initialized successfully." << std::endl;
    return true;
}

void AftermathCrashReporter::shutdown()
{
    if (s_initialized && pfnDisableGpuCrashDumps)
    {
        pfnDisableGpuCrashDumps();
        s_initialized = false;
        std::wcout << L"Aftermath GPU crash dump monitoring disabled." << std::endl;
    }

    if (hAftermathModule)
    {
        FreeLibrary(hAftermathModule);
        hAftermathModule = nullptr;
        pfnEnableGpuCrashDumps = nullptr;
        pfnDisableGpuCrashDumps = nullptr;
        pfnGetCrashDumpStatus = nullptr;
    }
}

bool AftermathCrashReporter::checkDeviceLost()
{
    if (!s_initialized || !pfnGetCrashDumpStatus)
    {
        return false;
    }

    GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
    const GFSDK_Aftermath_Result result = pfnGetCrashDumpStatus(&status);
    
    if (result != GFSDK_Aftermath_Result_Success)
    {
        return false;
    }

    // Return true if a crash has been detected
    return status != GFSDK_Aftermath_CrashDump_Status_NotStarted && 
           status != GFSDK_Aftermath_CrashDump_Status_Unknown;
}

void AftermathCrashReporter::waitForCrashDumpCollection()
{
    if (!s_initialized || !pfnGetCrashDumpStatus)
    {
        std::wcout << L"Crash dump collection not available." << std::endl;
        return;
    }

    std::wcout << L"Waiting for crash dump collection to complete..." << std::endl;
    
    GFSDK_Aftermath_CrashDump_Status status = GFSDK_Aftermath_CrashDump_Status_Unknown;
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(10); // 10 second timeout

    do
    {
        pfnGetCrashDumpStatus(&status);
        
        if (status == GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed ||
            status == GFSDK_Aftermath_CrashDump_Status_Finished)
        {
            break;
        }

        // Check for timeout
        auto currentTime = std::chrono::steady_clock::now();
        if (currentTime - startTime > timeout)
        {
            std::wcout << L"Timeout waiting for crash dump collection." << std::endl;
            break;
        }

        // Wait a bit before polling again
        Sleep(50);
    } while (true);

    std::wcout << L"Crash dump collection completed with status: " << status << std::endl;
}

void GFSDK_AFTERMATH_CALL AftermathCrashReporter::onCrashDump(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData)
{
    std::wcout << L"GPU crash detected! Saving crash dump..." << std::endl;
    
    // Save crash dump to file
    saveCrashDumpToFile(pGpuCrashDump, gpuCrashDumpSize);
    
    s_crashDumpCounter++;
    std::wcout << L"Crash dump saved. Total crashes: " << s_crashDumpCounter << std::endl;
}

void GFSDK_AFTERMATH_CALL AftermathCrashReporter::onShaderDebugInfo(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize, void* pUserData)
{
    std::wcout << L"Shader debug info available. Saving..." << std::endl;
    
    // Save shader debug info to file
    saveShaderDebugInfoToFile(pShaderDebugInfo, shaderDebugInfoSize);
    
    s_shaderDebugInfoCounter++;
}

void GFSDK_AFTERMATH_CALL AftermathCrashReporter::onDescription(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription addValue, void* pUserData)
{
    // Add application-specific information to crash dump
    addValue(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, "VulkanPlayGround");
    addValue(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, "1.0.0");
    
    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::string timestamp = std::to_string(time_t);
    addValue(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined + 1, timestamp.c_str());
}

void AftermathCrashReporter::saveCrashDumpToFile(const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize)
{
    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::string filename = "crash_dumps/crash_dump_" + std::to_string(time_t) + "_" + std::to_string(s_crashDumpCounter) + ".nv-gpudmp";
    
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open())
    {
        file.write(static_cast<const char*>(pGpuCrashDump), gpuCrashDumpSize);
        file.close();
        std::wcout << L"Crash dump saved to: " << filename.c_str() << std::endl;
    }
    else
    {
        std::wcout << L"Failed to save crash dump to file: " << filename.c_str() << std::endl;
    }
}

void AftermathCrashReporter::saveShaderDebugInfoToFile(const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize)
{
    // Generate filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::string filename = "crash_dumps/shader_debug_" + std::to_string(time_t) + "_" + std::to_string(s_shaderDebugInfoCounter) + ".nvdbg";
    
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open())
    {
        file.write(static_cast<const char*>(pShaderDebugInfo), shaderDebugInfoSize);
        file.close();
        std::wcout << L"Shader debug info saved to: " << filename.c_str() << std::endl;
    }
    else
    {
        std::wcout << L"Failed to save shader debug info to file: " << filename.c_str() << std::endl;
    }
}

// Existing NsightDebugger implementation
static void ReportInjectionError(const wchar_t* pMessage)
{
    OutputDebugStringW(pMessage);

    const auto pTitle = L"Injection Failure.";
    MessageBoxW(NULL, pMessage, pTitle, MB_OK | MB_ICONERROR);
}
bool NsightDebugger::initInjection()
{
     // Injecting into a process follows this basic flow:
    //
    // 1) Enumerating/detecting the installations installed on the machine
    // 2) Choosing a particular installation to use
    // 3) Determining the activities/capabilities of the particular installation
    // 4) Choosing a particular activity to use
    // 5) Injecting into the application, checking for success

    // 1) First, find Nsight Graphics installations
    uint32_t numInstallations = 0;
    auto result = NGFX_Injection_EnumerateInstallations(&numInstallations, nullptr);
    if (numInstallations == 0 || NGFX_INJECTION_RESULT_OK != result)
    {
        std::wstringstream stream;
        stream << L"Could not find any Nsight Graphics installations to inject: " << result << "\n";
        stream << L"Please install Nsight Graphics to enable programmatic injection.";
        ReportInjectionError(stream.str().c_str());
        return false;
    }

    std::vector<NGFX_Injection_InstallationInfo> installations(numInstallations);
    result = NGFX_Injection_EnumerateInstallations(&numInstallations, installations.data());
    if (numInstallations == 0 || NGFX_INJECTION_RESULT_OK != result)
    {
        std::wstringstream stream;
        stream << L"Could not find any Nsight Graphics installations to inject: " << result << "\n";
        stream << L"Please install Nsight Graphics to enable programmatic injection.";
        ReportInjectionError(stream.str().c_str());
        return false;
    }

    // 2) We have at least one Nsight Graphics installation, find which
    // activities are available using the latest installation.
    const NGFX_Injection_InstallationInfo& installation = installations.back();

    // 3) Retrieve the count of activities so we can initialize our activity data to the correct size
    uint32_t numActivities = 0;
    result = NGFX_Injection_EnumerateActivities(&installation, &numActivities, nullptr);
    if (numActivities == 0 || NGFX_INJECTION_RESULT_OK != result)
    {
        std::wstringstream stream;
        stream << L"Could not find any activities in Nsight Graphics installation: " << result << "\n";
        stream << L"Please install Nsight Graphics to enable programmatic injection.";
        ReportInjectionError(stream.str().c_str());
        return false;
    }

    // With the count of activities available, query their description
    std::vector<NGFX_Injection_Activity> activities(numActivities);
    result = NGFX_Injection_EnumerateActivities(&installation, &numActivities, activities.data());
    if (NGFX_INJECTION_RESULT_OK != result)
    {
        std::wstringstream stream;
        stream << L"NGFX_Injection_EnumerateActivities failed with" << result;
        ReportInjectionError(stream.str().c_str());
        return false;
    }

    // 4) We have valid activities. From here, we choose an activity.
    // In this sample, we use "Frame Debugger" activity
    const NGFX_Injection_Activity* pActivityToInject = nullptr;
    for (const NGFX_Injection_Activity& activity : activities)
    {
        if (activity.type == NGFX_INJECTION_ACTIVITY_FRAME_DEBUGGER)
        {
            pActivityToInject = &activity;
            break;
        }
    }

    if (!pActivityToInject) {
        std::wstringstream stream;
        stream << L"Frame Debugger activity is not available" << result;
        ReportInjectionError(stream.str().c_str());
        return false;
    }

    // 5) With the activity identified, Inject into the process, setup for the
    // Frame Debugger activity
    result = NGFX_Injection_InjectToProcess(&installation, pActivityToInject);
    if (NGFX_INJECTION_RESULT_OK != result)
    {
        std::wstringstream stream;
        stream << L"NGFX_Injection_InjectToProcess failed with" << result;
        ReportInjectionError(stream.str().c_str());
        return false;
    }

    // Everything succeeded
    return true;
}
void NsightDebugger::capture()
{
    NGFX_Injection_ExecuteActivityCommand();
}
}