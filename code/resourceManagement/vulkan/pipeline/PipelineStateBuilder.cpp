#include "PipelineStateBuilder.h"
#include "Material.h"

namespace Play
{
PipelineStateBuilder::PipelineStateBuilder(PipelineCacheManager& pipelineCache, DescriptorSetCache& descriptorCache,
                                           const PipelineLayoutDesc& layoutDesc, const RenderPass* renderPass)
    : _pipelineCache(pipelineCache), _descriptorCache(descriptorCache), _layoutDesc(layoutDesc), _renderPass(renderPass)
{
}

GraphicsPipelineStateInitializer PipelineStateBuilder::build(const GraphicsPipelineStateInitializer& defaults, const MaterialInstance* material) const
{
    if (!_renderPass)
    {
        throw std::runtime_error("Graphics render pass is not prepared");
    }

    auto              initializer = defaults;
    const auto&       config      = _renderPass->getConfig();
    RenderTargetState renderTargets;
    for (const auto& attachment : config.colorAttachments)
    {
        renderTargets.colorFormats.push_back(attachment.format);
    }
    if (config.depthAttachment) renderTargets.depthAttachmentFormat = config.depthAttachment->format;
    if (config.stencilAttachment) renderTargets.stencilAttachmentFormat = config.stencilAttachment->format;

    if (!config.colorAttachments.empty())
        renderTargets.sampleCount = config.colorAttachments.front().samples;
    else if (config.depthAttachment)
        renderTargets.sampleCount = config.depthAttachment->samples;
    else if (config.stencilAttachment)
        renderTargets.sampleCount = config.stencilAttachment->samples;

    initializer.setRenderTargetState(renderTargets);

    if (material)
    {
        DescriptorSetBindings bindings;
        if (!material->buildDescriptorBindings(bindings))
        {
            throw std::runtime_error("Material has missing or unsupported descriptor resources");
        }

        const auto& shaders = material->getMaterial()->getShaderSet();
        if (shaders.hasShader(ShaderStage::eRayMesh))
            initializer.setMeshShader(shaders.getShader(ShaderStage::eRayMesh), shaders.getShader(ShaderStage::eFragment),
                                      shaders.getShader(ShaderStage::eRayTask));
        else
            initializer.setShader(shaders.getShader(ShaderStage::eVertex), shaders.getShader(ShaderStage::eFragment));

        material->applyToPSOState(initializer.psoState);
        initializer.materialDescriptorSet = _descriptorCache.getOrCreateDescriptorSet(bindings);
    }

    initializer.pipelineLayout =
        buildPipelineLayout(initializer.materialDescriptorSet, initializer.hasPushConstantRange, initializer.pushConstantRange);
    return initializer;
}

ComputePipelineStateInitializer PipelineStateBuilder::build(const ComputePipelineStateInitializer& defaults) const
{
    auto initializer = defaults;
    initializer.pipelineLayout =
        buildPipelineLayout(initializer.materialDescriptorSet, initializer.hasPushConstantRange, initializer.pushConstantRange);
    return initializer;
}

PipelineLayout* PipelineStateBuilder::buildPipelineLayout(const CommonDescriptorSet& materialSet, bool hasPushConstantRange,
                                                          const VkPushConstantRange& pushConstantRange) const
{
    PipelineLayoutDesc layoutDesc = _layoutDesc;
    if (materialSet.layout != VK_NULL_HANDLE) layoutDesc.setDescriptorSetLayout(DescriptorEnum::eDrawObjectDescriptorSet, materialSet.layout);
    if (hasPushConstantRange) layoutDesc.setPushConstantRange(pushConstantRange);
    return _pipelineCache.getOrCreatePipelineLayout(layoutDesc);
}
} // namespace Play
