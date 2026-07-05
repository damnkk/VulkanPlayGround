#ifndef PROFILING_H
#define PROFILING_H

// Lightweight profiling facade for CPU scopes and Vulkan command labels.
//
// Usage:
//   PLAY_PROFILE_SCOPE("RDG Compile");
//   PLAY_PROFILE_FUNCTION();
//   PLAY_PROFILE_MARK("Frame Warmup Done");
//
//   PLAY_PROFILE_COMMAND_LABEL(cmd, "GBufferPass");
//   PLAY_PROFILE_COMMAND_LABEL_COLOR(cmd, "Gaussian Sort", 0.9f, 0.6f, 0.1f, 1.0f);
//
// CPU scopes use NVTX ranges and optional log timing. They show up on the CPU thread
// timeline in Nsight Systems.
//
// Command labels use VK_EXT_debug_utils labels. They label recorded Vulkan command
// buffer work and show up around Vulkan/GPU work in Nsight tools.
//
// Local switch:
//   Set PLAY_ENABLE_PROFILING to 1 while profiling, then rebuild.
//   Keep it 0 for normal development and release packaging.
//
// Release and MinSizeRel builds define NDEBUG, so profiling is still forced off there
// unless PLAY_ENABLE_PROFILING_IN_RELEASE is also set to 1.

#include <cstdint>
#include <volk.h>

#ifndef PLAY_ENABLE_PROFILING
#define PLAY_ENABLE_PROFILING 1
#endif

#ifndef PLAY_ENABLE_PROFILING_IN_RELEASE
#define PLAY_ENABLE_PROFILING_IN_RELEASE 0
#endif

#if PLAY_ENABLE_PROFILING && defined(NDEBUG) && !PLAY_ENABLE_PROFILING_IN_RELEASE
#undef PLAY_ENABLE_PROFILING
#define PLAY_ENABLE_PROFILING 0
#endif

#ifndef PLAY_ENABLE_NVTX
#define PLAY_ENABLE_NVTX PLAY_ENABLE_PROFILING
#endif

#ifndef PLAY_ENABLE_PROFILE_LOG
#define PLAY_ENABLE_PROFILE_LOG 0
#endif

namespace Play
{
#if PLAY_ENABLE_PROFILING
class ScopedProfileZone
{
public:
    explicit ScopedProfileZone(const char* name);
    ~ScopedProfileZone();

    ScopedProfileZone(const ScopedProfileZone&)            = delete;
    ScopedProfileZone& operator=(const ScopedProfileZone&) = delete;
    ScopedProfileZone(ScopedProfileZone&&)                 = delete;
    ScopedProfileZone& operator=(ScopedProfileZone&&)      = delete;

private:
    const char* _name    = nullptr;
    uint64_t    _startNs = 0;
};

class ScopedCommandLabel
{
public:
    ScopedCommandLabel(VkCommandBuffer cmd, const char* name);
    ScopedCommandLabel(VkCommandBuffer cmd, const char* name, float red, float green, float blue, float alpha = 1.0f);
    ~ScopedCommandLabel();

    ScopedCommandLabel(const ScopedCommandLabel&)            = delete;
    ScopedCommandLabel& operator=(const ScopedCommandLabel&) = delete;
    ScopedCommandLabel(ScopedCommandLabel&&)                 = delete;
    ScopedCommandLabel& operator=(ScopedCommandLabel&&)      = delete;

private:
    VkCommandBuffer _cmd    = VK_NULL_HANDLE;
    bool            _active = false;
};

void markProfileEvent(const char* name);
#else
class ScopedProfileZone
{
public:
    explicit ScopedProfileZone(const char*) {}
};

class ScopedCommandLabel
{
public:
    ScopedCommandLabel(VkCommandBuffer, const char*) {}
    ScopedCommandLabel(VkCommandBuffer, const char*, float, float, float, float = 1.0f) {}
};

inline void markProfileEvent(const char*) {}
#endif

using ScopedTimer = ScopedProfileZone;
} // namespace Play

#define PLAY_PROFILE_CONCAT_INNER(a, b) a##b
#define PLAY_PROFILE_CONCAT(a, b) PLAY_PROFILE_CONCAT_INNER(a, b)

#if PLAY_ENABLE_PROFILING
#define PLAY_PROFILE_SCOPE(name) ::Play::ScopedProfileZone PLAY_PROFILE_CONCAT(playProfileZone_, __LINE__)(name)
// naming scope using current function
#define PLAY_PROFILE_FUNCTION() PLAY_PROFILE_SCOPE(__FUNCTION__)
#define PLAY_PROFILE_COMMAND_LABEL(cmd, name) ::Play::ScopedCommandLabel PLAY_PROFILE_CONCAT(playCommandLabel_, __LINE__)(cmd, name)
#define PLAY_PROFILE_COMMAND_LABEL_COLOR(cmd, name, red, green, blue, alpha) \
    ::Play::ScopedCommandLabel PLAY_PROFILE_CONCAT(playCommandLabel_, __LINE__)(cmd, name, red, green, blue, alpha)
// hitpoint, prove that code is executed
#define PLAY_PROFILE_MARK(name) ::Play::markProfileEvent(name)
#else
#define PLAY_PROFILE_SCOPE(name) ((void) 0)
#define PLAY_PROFILE_FUNCTION() ((void) 0)
#define PLAY_PROFILE_COMMAND_LABEL(cmd, name) ((void) 0)
#define PLAY_PROFILE_COMMAND_LABEL_COLOR(cmd, name, red, green, blue, alpha) ((void) 0)
#define PLAY_PROFILE_MARK(name) ((void) 0)
#endif

#define PLAY_SCOPE_TIMER(name) PLAY_PROFILE_SCOPE(name)

#endif // PROFILING_H
