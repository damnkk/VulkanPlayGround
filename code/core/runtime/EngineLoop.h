#pragma once

#include "RuntimeConfig.h"

#include <nvvk/context.hpp>

namespace Play::runtime
{

class EngineLoop
{
public:
    int run(const RuntimeConfig& config, const nvvk::ContextInitInfo& contextInfo);
};

} // namespace Play::runtime
