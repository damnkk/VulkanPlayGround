#ifndef RDG_BARRIER_UTILS_H
#define RDG_BARRIER_UTILS_H

#include <vulkan/vulkan.h>

namespace Play
{
bool isImageBarrierValid(const VkImageMemoryBarrier2& barrier);
bool isBufferBarrierValid(const VkBufferMemoryBarrier2& barrier);

VkFlags pipelineStageToShaderStage(VkPipelineStageFlags2 pipelineStage);
VkFlags inferShaderStageFromPipelineStage(VkFlags64 pipelineStage);
} // namespace Play

#endif // RDG_BARRIER_UTILS_H
