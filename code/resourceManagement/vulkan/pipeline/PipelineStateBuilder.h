#ifndef PLAY_PIPELINE_STATE_BUILDER_H
#define PLAY_PIPELINE_STATE_BUILDER_H

#include "PipelineCacheManager.h"

namespace Play
{
class MaterialInstance;

// Pass-local preparation. Material references stop here; binding consumes the resolved initializer.
class PipelineStateBuilder
{
public:
    PipelineStateBuilder(PipelineCacheManager& pipelineCache, DescriptorSetCache& descriptorCache, const PipelineLayoutDesc& layoutDesc,
                         const RenderPass* renderPass);

    GraphicsPipelineStateInitializer build(const GraphicsPipelineStateInitializer& defaults, const MaterialInstance* material = nullptr) const;
    ComputePipelineStateInitializer  build(const ComputePipelineStateInitializer& defaults) const;

private:
    PipelineLayout* buildPipelineLayout(const CommonDescriptorSet& materialSet, bool hasPushConstantRange,
                                        const VkPushConstantRange& pushConstantRange) const;

    PipelineCacheManager& _pipelineCache;
    DescriptorSetCache&   _descriptorCache;
    PipelineLayoutDesc    _layoutDesc;
    const RenderPass*     _renderPass;
};
} // namespace Play

#endif // PLAY_PIPELINE_STATE_BUILDER_H
