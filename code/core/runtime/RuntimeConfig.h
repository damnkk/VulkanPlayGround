#pragma once

#include "pch.h"
#include <string>
namespace Play::runtime
{

struct RuntimeConfig
{
    const char* windowTitle = "VulkanPlayGround SDL Runtime";
    uint32_t    width       = 1280;
    uint32_t    height      = 720;
    bool        vSync       = true;
    bool        validation  = false;
    bool        verbose     = false;
    std::string renderMode  = "defer";
};

} // namespace Play::runtime
