#include "PostProcessPass.h"
#include "DeferRendering.h"
#include "RDG/RDG.h"
#include "PlayProgram.h"
#include "ShaderManager.hpp"
#include "VulkanDriver.h"
#include "resourceManagement/PconstantType.h.slang"
#include "PlayAllocator.h"

#include "GBufferConfig.h"
namespace Play
{
void PostProcessPass::init()
{
    uint32_t PostProcesscId =
        ShaderManager::Instance().loadShaderFromFile("postProcessComp", "tonemapper.slang", ShaderStage::eCompute, ShaderType::eSLANG, "Tonemap");
    auto shadermodule = ShaderManager::Instance().getShaderById(PostProcesscId);

    _postProgram = ProgramPool::Instance().alloc<ComputeProgram>();
    _postProgram->setComputeModuleID(PostProcesscId);
    _postProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();
    _tonemapper.init(&PlayResourceManager::Instance(), {shadermodule->_spvCode});
}

void PostProcessPass::build(RDG::RDGBuilder* rdgBuilder)
{
    auto inputTextureRef = rdgBuilder->getTexture("LightPassOutput");
    auto outputTexRef    = rdgBuilder->createTexture("outputTexture")
                            .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                            .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                            .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                            .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                            .MipmapLevel(1)
                            .finish();
    rdgBuilder->registTexture(outputTexRef);
    DeferRenderer* ownedRender = static_cast<DeferRenderer*>(_ownedRender);
    auto           pass        = rdgBuilder->createComputePass("postProcessPass")
                    .read(0, inputTextureRef, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
                    .readWrite(1, outputTexRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
                    .execute(
                        [inputTextureRef, outputTexRef, this](RDG::PassNode* passNode, RDG::RenderContext& context)
                        {
                            PlayResourceManager::Instance().acquireSampler(inputTextureRef->getRHI()->descriptor.sampler);
                            this->_tonemapper.runCompute(context._currCmdBuffer, vkDriver->getViewportSize(),
                                                         vkDriver->getTonemapperControlComponent().getCPUHandle(),
                                                         inputTextureRef->getRHI()->descriptor, outputTexRef->getRHI()->descriptor);
                        })
                    .finish();
}

PostProcessPass::PostProcessPass(DeferRenderer* ownedRender) : _ownedRender(ownedRender) {}

PostProcessPass::~PostProcessPass()
{
    ProgramPool::Instance().free(_postProgram);
    _tonemapper.deinit();
}

} // namespace Play