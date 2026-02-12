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
    uint32_t PostProcesscId = ShaderManager::Instance().loadShaderFromFile("postProcessComp", "newShaders/postProcess.comp.slang",
                                                                           ShaderStage::eCompute, ShaderType::eSLANG, "main");
    auto     shadermodule   = ShaderManager::Instance().getShaderById(PostProcesscId);

    _postProgram = ProgramPool::Instance().alloc<ComputeProgram>();
    _postProgram->setComputeModuleID(PostProcesscId);
    _postProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();
    //_tonemapper.init(&PlayResourceManager::Instance(), {shadermodule->_spvCode});
}

void PostProcessPass::build(RDG::RDGBuilder* rdgBuilder)
{
    auto inputTexture = rdgBuilder->getTexture("LightPassOutput");
    auto outputTexRef = rdgBuilder->createTexture("outputTexture")
                            .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                            .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                            .Format(VK_FORMAT_R8G8B8A8_UNORM)
                            .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                            .MipmapLevel(1)
                            .finish();
    rdgBuilder->registTexture(outputTexRef);
    DeferRenderer* ownedRender = static_cast<DeferRenderer*>(_ownedRender);
    auto           pass        = rdgBuilder->createComputePass("postProcessPass")
                    .read(0, inputTexture, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
                    .readWrite(1, outputTexRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
                    .execute(
                        [ownedRender, this](RDG::PassNode* passNode, RDG::RenderContext& context)
                        {
                            VkCommandBuffer   cmd = context._currCmdBuffer;
                            PerFrameConstant* perFrameConstant =
                                this->_postProgram->getDescriptorSetManager().getPushConstantData<PerFrameConstant>();
                            perFrameConstant->cameraBufferDeviceAddress = ownedRender->getCurrentCameraBuffer()->address;
                            this->_postProgram->setPassNode(static_cast<RDG::ComputePassNode*>(passNode));
                            this->_postProgram->bind(cmd);
                            context._pendingComputeState->bindDescriptorSet(cmd, this->_postProgram);
                            vkCmdDispatch(cmd, (vkDriver->getViewportSize().width + 15) / 16, (vkDriver->getViewportSize().height + 15) / 16, 1);
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