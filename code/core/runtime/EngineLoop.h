#ifndef PLAY_CODE_CORE_RUNTIME_ENGINELOOP_H
#define PLAY_CODE_CORE_RUNTIME_ENGINELOOP_H


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

#endif // PLAY_CODE_CORE_RUNTIME_ENGINELOOP_H
