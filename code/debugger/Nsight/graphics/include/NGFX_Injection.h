//--------------------------------------------------------------------------------------
// Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto.  Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//--------------------------------------------------------------------------------------
// File: NGFX_Injection.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_INJECTION_H_DEF
#define NGFX_INJECTION_H_DEF

/**
 * @file NGFX_Injection.h
 */

#include <stdint.h>

/**
 * @defgroup INJECTION_API Injection API
 *
 * The Injection API contains methods for programmatic injection of Nsight tools
 * from target applications. This provides an alternative mechanism to the
 * Nsight Host for launching and injecting applications.
 *
 * @{
 */

#if defined(__cplusplus) && defined(_WIN32)
#define NGFX_Injection_API __declspec(dllexport)
#else
#define NGFX_Injection_API
#endif

#define NGFX_Injection_API_VersionMajor 0
#define NGFX_Injection_API_VersionMinor 8
#define NGFX_Injection_API_VersionPatch 0
#define NGFX_Injection_API_Version ((NGFX_Injection_API_VersionMajor * 1000) + (NGFX_Injection_API_VersionMinor * 10) + NGFX_Injection_API_VersionPatch)

#define NGFX_Injection_API_MK_NUM(major, minor, patch) major##.##minor##.##patch
#define NGFX_Injection_API_MK_STR_INNER(arg) #arg
#define NGFX_Injection_API_MK_STR(arg) NGFX_Injection_API_MK_STR_INNER(arg)
#define NGFX_Injection_API_VersionString NGFX_Injection_API_MK_STR(NGFX_Injection_API_VersionMajor) "." NGFX_Injection_API_MK_STR(NGFX_Injection_API_VersionMinor) "." NGFX_Injection_API_MK_STR(NGFX_Injection_API_VersionPatch)

#ifdef _WIN32
typedef wchar_t NGFX_Injection_PathChar;
#else
typedef char NGFX_Injection_PathChar;
#endif

/**
 * @brief Reports success or failure of an injection operation
 *
 * All functions within the NGFX Injection API will report a result code that
 * idenfies the success or failure of the operation. Negative values generally
 * indicate a failed operation.
 */
typedef enum NGFX_Injection_Result
{
    NGFX_INJECTION_RESULT_OK = 0,                   ///< Success
    NGFX_INJECTION_RESULT_FAILURE = -1,             ///< Generic failure
    NGFX_INJECTION_RESULT_INVALID_ARGUMENT = -2,    ///< Invalid argument
    NGFX_INJECTION_RESULT_INJECTION_FAILED = -3,    ///< Generic injection failure
    NGFX_INJECTION_RESULT_ALREADY_INJECTED = -4,    ///< Cannot inject because already injected
    NGFX_INJECTION_RESULT_NOT_INJECTED = -5,        ///< Cannot execute activity because not injected
    NGFX_INJECTION_RESULT_DRIVER_STILL_LOADED = -6, ///< Cannot inject because a graphics driver is already or still loaded.
    NGFX_INJECTION_RESULT_INVALID_PROJECT = -7,     ///< Project file specified was malformed
    NGFX_INJECTION_RESULT_INJECTION_REJECTED = -8,  ///< Cannot inject this type
} NGFX_Injection_Result;

/**
 * @brief Identifies the SKU of a particular Nsight installation
 *
 * The sku is the distribution of Nsight.
 */
typedef enum NGFX_Nsight_SKU
{
    NGFX_NSIGHT_SKU_UNKNOWN,
    NGFX_NSIGHT_SKU_PUBLIC,
    NGFX_NSIGHT_SKU_PRO,
    NGFX_NSIGHT_SKU_INTERNAL,
} NGFX_Nsight_SKU;

/**
 * @brief Reports information about a particular Nsight installation
 *
 * The NGFX_Injection_InstallationInfo structure contains information that
 * uniquely identifies a particular installation, allowing you to choose which
 * particular installation to inject into.
 */
typedef struct NGFX_Injection_InstallationInfo
{
    NGFX_Nsight_SKU sku;                             ///< The distribution of Nsight in this installation
    uint16_t versionMajor;                           ///< The major version of the Nsight installation
    uint16_t versionMinor;                           ///< The minor version of the Nsight installation
    uint16_t versionPatch;                           ///< The patch version of the Nsight installation
    const NGFX_Injection_PathChar* installationPath; ///< The path on the system to the Nsight installation
} NGFX_Injection_InstallationInfo;

/**
 * @brief Specifies a particular activity that is available from Nsight Graphics
 *
 * The activity specifies the type of operations that can be performed when
 * analyzing the application. Only one activity may be selected for a given
 * session.
 */
typedef enum NGFX_Injection_ActivityType
{
    NGFX_INJECTION_ACTIVITY_UNKNOWN = 0,
    NGFX_INJECTION_ACTIVITY_FRAME_DEBUGGER = 1,
    //NGFX_INJECTION_ACTIVITY_FRAME_PROFILER = 2, // this activity has been removed from Nsight Graphics
    NGFX_INJECTION_ACTIVITY_GENERATE_CPP_CAPTURE = 3,
    NGFX_INJECTION_ACTIVITY_GPU_TRACE = 4,
    NGFX_INJECTION_ACTIVITY_PYLON_CAPTURE = 5,
    NGFX_INJECTION_ACTIVITY_SHADER_DEBUGGER = 6,
} NGFX_Injection_ActivityType;

/**
 * @brief Reports information about a specified activity
 *
 * This structure describes an injection activity.
 */
typedef struct NGFX_Injection_Activity
{
    NGFX_Injection_ActivityType type; ///< The activity type
    const char* description;          ///< A text description of the activity
} NGFX_Injection_Activity;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumerates the installations of Nsight that are installed on the current system
 *
 * When Nsight Graphics is installed, this method will discover one or more
 * installations and report them for possible injection. When no installations
 * are present, pCount will be assigned to 0. It is the responsibility of the
 * user application to point pInstallations to valid memory for writing.
 *
 * @param pCount - Returns the count of Nsight installations found on the system.
 * @param pInstallations - Returns the list of Nsight installation. This
 * parameter may be null when querying the installation count. If non-null the
 * array of installations will be written here.
 *
 * @return An injection result that reports whether the installations could be
 * queried
 */
NGFX_Injection_API NGFX_Injection_Result NGFX_Injection_EnumerateInstallations(uint32_t* pCount, NGFX_Injection_InstallationInfo* pInstallations);

/**
 * @brief Enumerates the activities that are available for the particular installation
 * of Nsight Graphics.
 *
 * This method must be called to identify the activities that may be injected
 * for a particular installation of Nsight Graphics.  If installation is valid,
 * a count of activities will be returned and when pActivities is non-null, a
 * list of activities of size *pCount will be populated. It is the
 * responsibilities of the user application to point pActivities to valid memory
 * for writing.
 *
 * @param pInstallation - A pointer to an installation that was previously
 * identified by @ref NGFX_Injection_EnumerateInstallations.
 * @param pCount - Returns the count of activities available for the specified
 * installation
 * @param pActivities - Returns the list of activities for the specified
 * installation. This parameter may be null when querying the installation
 * count. If non-null the array of activities will be written here.
 *
 * @return An injection result that reports whether the activities could be
 * queried
 */
NGFX_Injection_API NGFX_Injection_Result NGFX_Injection_EnumerateActivities(const NGFX_Injection_InstallationInfo* pInstallation, uint32_t* pCount, NGFX_Injection_Activity* pActivities);

/**
 * @brief Globally configures the activity settings based off an existing Nsight
 * Graphics project file.
 *
 * @param pInstallation - A pointer to an installation that was previously
 * identified by @ref NGFX_Injection_EnumerateInstallations.
 * @param pProjectFilePath - Path of an Nsight Graphics project file to load. This file is
 * created by the Nsight Graphics UI and saved for consumption by the NGFX Injection API.
 *
 * @return An injection result that reports whether the activities settings
 * object was successfully created.
 */
NGFX_Injection_API NGFX_Injection_Result NGFX_Injection_SetActivitySettingsFromProjectFile(const NGFX_Injection_InstallationInfo* pInstallation, const NGFX_Injection_PathChar* pProjectFilePath);

/**
 * @brief Globally configures the activity settings based off the contents
 * of an existing Nsight Graphics project file.
 *
 * @param pInstallation - A pointer to an installation that was previously
 * identified by @ref NGFX_Injection_EnumerateInstallations.
 * @param pJson - JSON contents of an Nsight Graphics project file to load.
 *
 * @return An injection result that reports whether the activities settings
 * object was successfully created.
 */
NGFX_Injection_API NGFX_Injection_Result NGFX_Injection_SetActivitySettingsFromProjectJson(const NGFX_Injection_InstallationInfo* pInstallation, const char* pJson);

/**
 * @brief Performs an injection into the current process with the specified
 * installation and activity.
 *
 * To perform an injection into the current process, a valid installation and
 * activity must have been queried. Additionally, the NVIDIA driver must not
 * be actively loaded, otherwise @ref NGFX_INJECTION_RESULT_DRIVER_STILL_LOADED
 * will be returned.
 *
 * @param pInstallation - A pointer to an installation that was previously
 * identified by @ref NGFX_Injection_EnumerateInstallations.
 * @param pActivity - A pointer to the activity to select when injecting that was
 * queried by @ref NGFX_Injection_EnumerateActivities.
 *
 * @return An injection result that reports whether the process could be
 * injected.
 */
NGFX_Injection_API NGFX_Injection_Result NGFX_Injection_InjectToProcess(const NGFX_Injection_InstallationInfo* pInstallation, const NGFX_Injection_Activity* pActivity);

/**
 * @brief Performs the primary command for the injected activity.
 *
 * This command is typically a 'capture' command.  The prerequisite for this is
 * that NGFX_Injection_InjectToProcess succeeded, i.e., that the application has
 * previously selected a valid installation and activity and successfully
 * injected via @ref NGFX_Injection_InjectToProcess.
 *
 * This function is currently experimental and may be revised, replaced, or
 * eliminated in a future version.
 */
NGFX_Injection_API NGFX_Injection_Result NGFX_Injection_ExecuteActivityCommand();

#ifdef __cplusplus
} // extern "C"
#endif

/**
 * END defgroup INJECTION_API
 * @}
 */

#endif // #ifndef NGFX_INJECTION_H_DEF
