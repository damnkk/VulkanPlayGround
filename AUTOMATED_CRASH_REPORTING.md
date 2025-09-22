# Automated Crash Reporting (自动化CR)

## Overview

This document describes the automated crash reporting system implemented in VulkanPlayGround using NVIDIA Aftermath GPU crash dump technology.

## Features

### 1. Automatic GPU Crash Detection
- Monitors Vulkan API calls for GPU crashes and hangs
- Detects device lost conditions automatically
- Integrates with the main application render loop

### 2. Crash Dump Collection
- Automatically saves GPU crash dumps when they occur
- Collects shader debug information
- Saves files to `crash_dumps/` directory with timestamps
- Includes application metadata in crash dumps

### 3. Graceful Crash Handling
- Waits for crash dump collection to complete before shutdown
- Provides status monitoring during crash processing
- Logs crash events with timestamps

## Implementation Details

### AftermathCrashReporter Class

The `AftermathCrashReporter` class provides the main interface for automated crash reporting:

```cpp
// Initialize crash reporting (call before Vulkan device creation)
bool AftermathCrashReporter::initialize();

// Shutdown crash reporting (call during application cleanup)
void AftermathCrashReporter::shutdown();

// Check if a device lost condition indicates a crash
bool AftermathCrashReporter::checkDeviceLost();

// Wait for crash dump collection to complete
void AftermathCrashReporter::waitForCrashDumpCollection();
```

### Integration Points

1. **Application Initialization** (`PlayApp::OnInit()`):
   - Crash reporting is initialized before any Vulkan device creation
   - Creates crash dump storage directory

2. **Main Render Loop** (`PlayApp::Run()`):
   - Checks for device lost conditions each frame
   - Automatically triggers crash dump collection when needed
   - Gracefully exits the render loop on crash detection

3. **Application Shutdown** (`PlayApp::onDestroy()`):
   - Properly shuts down crash reporting system
   - Ensures all resources are cleaned up

### File Output

Crash dumps are saved in the following format:
- **GPU Crash Dumps**: `crash_dumps/crash_dump_{timestamp}_{counter}.nv-gpudmp`
- **Shader Debug Info**: `crash_dumps/shader_debug_{timestamp}_{counter}.nvdbg`

## Dependencies

### Required Libraries
- **GFSDK_Aftermath_Lib.dll**: NVIDIA Aftermath runtime library
- **NVIDIA GPU Driver**: R495 or later recommended for full feature support

### Dynamic Loading
The implementation uses dynamic loading of Aftermath libraries:
- Gracefully handles missing libraries (crash reporting disabled)
- No compilation dependencies on Aftermath static libraries
- Runtime detection of Aftermath availability

## Usage

### Basic Integration

```cpp
#include "debugger/debugger.h"

// In application initialization
AftermathCrashReporter::initialize();

// In main loop
if (checkForDeviceLost()) {
    AftermathCrashReporter::waitForCrashDumpCollection();
    // Handle crash gracefully
}

// In application shutdown
AftermathCrashReporter::shutdown();
```

### Configuration

The crash reporter is configured for:
- **Vulkan API monitoring**: Tracks Vulkan-specific crashes
- **Deferred debug info**: Collects shader debug info only when crashes occur
- **Automatic metadata**: Includes application name, version, and timestamps

## Troubleshooting

### Library Not Found
If you see "Aftermath library not found" messages:
1. Ensure NVIDIA Aftermath SDK is installed
2. Verify `GFSDK_Aftermath_Lib.dll` is in the system PATH or application directory
3. Check that you have an NVIDIA GPU with compatible drivers

### No Crash Dumps Generated
1. Verify Aftermath initialization succeeded
2. Check that the `crash_dumps/` directory is writable
3. Ensure the GPU crash actually triggers Aftermath detection

### Performance Considerations
- Aftermath monitoring has minimal performance impact during normal operation
- Crash dump collection occurs only when crashes happen
- Deferred debug info collection reduces memory overhead

## API Reference

### Status Codes
- `GFSDK_Aftermath_CrashDump_Status_NotStarted`: No crash detected
- `GFSDK_Aftermath_CrashDump_Status_CollectingData`: Crash detected, collecting data
- `GFSDK_Aftermath_CrashDump_Status_InvokingCallback`: Processing crash dump
- `GFSDK_Aftermath_CrashDump_Status_Finished`: Crash dump collection complete
- `GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed`: Collection failed

### Callback Functions
- `onCrashDump()`: Called when GPU crash dump is available
- `onShaderDebugInfo()`: Called when shader debug information is available
- `onDescription()`: Called to add custom metadata to crash dumps

## Benefits

1. **Automated Debugging**: No manual intervention required for crash data collection
2. **Production Ready**: Suitable for deployment in release builds
3. **Comprehensive Data**: Includes both crash dumps and shader debug information
4. **Minimal Overhead**: Only activates during actual crashes
5. **Professional Integration**: Uses industry-standard NVIDIA Aftermath technology

This automated crash reporting system significantly improves the debugging experience for GPU-related issues in Vulkan applications.