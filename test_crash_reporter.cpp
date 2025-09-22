#include "code/debugger/debugger.h"
#include <iostream>

// Simple compilation test for the automated crash reporting system
int main()
{
    std::cout << "Testing Aftermath Crash Reporter integration..." << std::endl;
    
    // Test initialization (will gracefully fail if libraries not available)
    bool initialized = Play::AftermathCrashReporter::initialize();
    
    if (initialized)
    {
        std::cout << "Aftermath crash reporting initialized successfully!" << std::endl;
        
        // Test status check
        bool deviceLost = Play::AftermathCrashReporter::checkDeviceLost();
        std::cout << "Device lost check: " << (deviceLost ? "true" : "false") << std::endl;
        
        // Shutdown
        Play::AftermathCrashReporter::shutdown();
        std::cout << "Aftermath crash reporting shutdown complete." << std::endl;
    }
    else
    {
        std::cout << "Aftermath crash reporting not available (libraries not found)." << std::endl;
    }
    
    std::cout << "Test completed successfully!" << std::endl;
    return 0;
}