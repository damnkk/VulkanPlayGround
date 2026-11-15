#include "RDGBarrierUtils.h"

namespace Play
{
VkFlags pipelineStageToShaderStage(VkPipelineStageFlags2 pipelineStage)
{
    switch (pipelineStage)
    {
        case VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        case VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT:
            return VK_SHADER_STAGE_TASK_BIT_EXT;
        case VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT:
            return VK_SHADER_STAGE_MESH_BIT_EXT;
        case VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR:
            return VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                   VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        case VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT:
            return VK_SHADER_STAGE_ALL_GRAPHICS;
        case VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT:
            return VK_SHADER_STAGE_ALL;
        default:
            return VK_SHADER_STAGE_ALL;
    }
}

VkFlags inferShaderStageFromPipelineStage(VkFlags64 pipelineStage)
{
    VkFlags shaderStage = 0;
    while (pipelineStage)
    {
        const VkFlags64 stageBit = pipelineStage & (~pipelineStage + 1);
        shaderStage |= pipelineStageToShaderStage(static_cast<VkPipelineStageFlagBits2>(stageBit));
        pipelineStage &= pipelineStage - 1;
    }
    return shaderStage;
}
} // namespace Play
