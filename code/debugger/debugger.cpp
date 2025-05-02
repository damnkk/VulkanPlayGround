#include "debugger.h"
#include "pch.h"
#include <windows.h>
#include <debugapi.h>
#include "Nsight/graphics/include/NGFX_Injection.h"
namespace Play
{
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