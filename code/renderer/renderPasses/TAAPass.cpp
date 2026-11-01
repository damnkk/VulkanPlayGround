#include "TAAPass.h"
#include "DeferRendering.h"
#include "GBufferConfig.h"
#include "PConstantType.h.slang"
#include "core/runtime/VulkanRuntime.h"
#include "core/runtime/ViewControl.h"
#include "resourceManagement/renderGraph/RDG.h"
#include "resourceManagement/vulkan/pipeline/ShaderManager.hpp"

namespace
{
constexpr uint32_t kTAAGroupSize = 8;
} // namespace

namespace Play
{

TAAPass::~TAAPass() = default;

void TAAPass::init()
{
    const uint32_t resolveShaderID =
        ShaderManager::Instance().loadShaderFromFile("taaResolveComp", "newShaders/deferRenderer/taa/taaResolve.comp.slang", ShaderStage::eCompute);
    _resolvePipeline.setShader(resolveShaderID);
    _resolvePipeline.setPushConstant<TAAPushConstant>();

    const uint32_t inverseTonemapShaderID = ShaderManager::Instance().loadShaderFromFile(
        "taaInverseTonemapComp", "newShaders/deferRenderer/taa/inverseTonemap.comp.slang", ShaderStage::eCompute);
    _inverseTonemapPipeline.setShader(inverseTonemapShaderID);
}

void TAAPass::ensureHistoryTexture()
{
    const VkExtent2D extent = vkDriver->getViewportSize();
    if (_historyTexture && _historyTexture->Extent().width == extent.width && _historyTexture->Extent().height == extent.height)
    {
        return;
    }

    // Imported textures must be in the layout the graph leaves them in at the
    // end of a frame, so the frame-to-frame barrier has a valid old layout.
    // History is written last, in GENERAL.
    _historyTexture              = RefPtr<Texture>(new Texture(extent.width, extent.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                               VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_GENERAL));
    _historyTexture->DebugName() = "TAAHistory";

    _historyValid = false;
}

void TAAPass::build(RDG::RDGBuilder* rdgBuilder)
{
    ensureHistoryTexture();

    const VkExtent2D extent  = vkDriver->getViewportSize();
    const uint32_t   groupsX = (extent.width + kTAAGroupSize - 1) / kTAAGroupSize;
    const uint32_t   groupsY = (extent.height + kTAAGroupSize - 1) / kTAAGroupSize;

    RDG::RDGTextureRef inputRef    = rdgBuilder->getTexture(INPUT_TEXTURE_NAME);
    RDG::RDGTextureRef velocityRef = rdgBuilder->getTexture(GBufferConfig::Get(GBufferType::GVelocity).debugName);
    RDG::RDGTextureRef historyRef  = rdgBuilder->createTexture("TAAHistory").Import(_historyTexture.get()).finish();
    RDG::RDGTextureRef resolvedRef = rdgBuilder->createTexture("TAAResolved")
                                         .Extent({extent.width, extent.height, 1})
                                         .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                         .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                                         .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                         .MipmapLevel(1)
                                         .finish();

    rdgBuilder->createComputePass("TAAResolvePass")
        .read(0, inputRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .read(1, velocityRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .read(2, historyRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageWrite(3, resolvedRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .execute(
            [this, groupsX, groupsY](RDG::PassNode*, RDG::RenderContext& context)
            {
                const auto& control = _ownedRender->getViewControl();
                if (!control.settings().taa.enabled)
                {
                    _historyValid = false;
                    return;
                }
                if (control.historyReset()) _historyValid = false;
                TAAPushConstant pushConstant{};
                pushConstant.currentWeight    = _historyValid ? control.settings().taa.currentWeight : 1.0f;
                pushConstant.jitterUV         = _ownedRender->getCurrentCameraData().jitterUV;
                pushConstant.previousJitterUV = _ownedRender->getCurrentCameraData().previousJitterUV;
                const auto pipeline           = context.createPipelineStateBuilder().build(_resolvePipeline);
                context.bindPipeline(pipeline);
                context.bindPushConstant(pushConstant);
                vkCmdDispatch(context._currCmdBuffer, groupsX, groupsY, 1);
                _historyValid = true;
            })
        .finish();

    rdgBuilder->createComputePass("TAAInverseTonemapPass")
        .read(0, resolvedRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        // Resolve has finished reading the scene neighborhood; it is safe to overwrite scene color now.
        .storageWrite(1, inputRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageWrite(2, historyRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .execute(
            [this, groupsX, groupsY](RDG::PassNode*, RDG::RenderContext& context)
            {
                if (!_ownedRender->getViewControl().settings().taa.enabled) return;
                const auto pipeline = context.createPipelineStateBuilder().build(_inverseTonemapPipeline);
                context.bindPipeline(pipeline);
                vkCmdDispatch(context._currCmdBuffer, groupsX, groupsY, 1);
            })
        .finish();
}

} // namespace Play
