#include "VolumeSkyPass.h"
#include "VulkanDriver.h"
#include "ShaderManager.hpp"
#include "RDG/RDG.h"
#include "DeferRendering.h"
namespace Play
{

void VolumeSkyPass::init()
{
    auto skyBoxvId = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    auto skyBoxfId = ShaderManager::Instance().loadShaderFromFile("skyBoxFragment", "skyBoxProgram.frag.slang", ShaderStage::eFragment);
    _skyBoxProgram = std::make_unique<RenderProgram>(vkDriver->_device);
    _skyBoxProgram->setFragModuleID(skyBoxfId);
    _skyBoxProgram->setVertexModuleID(skyBoxvId);
    _skyBoxProgram->psoState().rasterizationState.frontFace       = VK_FRONT_FACE_CLOCKWISE;
    _skyBoxProgram->psoState().rasterizationState.cullMode        = VK_CULL_MODE_NONE;
    _skyBoxProgram->psoState().depthStencilState.depthWriteEnable = VK_FALSE;
    _skyBoxProgram->psoState().depthStencilState.depthTestEnable  = VK_FALSE;
}

void VolumeSkyPass::build(RDG::RDGBuilder* rdgBuilder)
{
    DeferRenderer* ownedRender = static_cast<DeferRenderer*>(_ownedRender);
    auto           SkyBoxRT    = rdgBuilder->createTexture("SkyBoxRT")
                        .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                        .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                        .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                        .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                        .MipmapLevel(1)
                        .finish();
    rdgBuilder->registTexture(SkyBoxRT);
    auto skyBoxPass =
        rdgBuilder->createRenderPass("skyBoxPass")
            .color(0, SkyBoxRT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .program(_skyBoxProgram.get())
            .execute(
                [ownedRender](RDG::PassNode* passNode, RDG::RenderContext& context)
                {
                    VkCommandBuffer  cmd = context._currCmdBuffer;
                    PerFrameConstant perFrameConstant;
                    perFrameConstant.cameraBufferDeviceAddress = ownedRender->getCurrentCameraBuffer()->address;
                    passNode->getProgram()->getDescriptorSetManager().setConstantRange(perFrameConstant);
                    passNode->getProgram()->bind(cmd);
                    context._pendingGfxState->bindDescriptorSet(cmd, passNode->getProgram());
                    VkViewport viewport = {0, 0, (float) vkDriver->getViewportSize().width, (float) vkDriver->getViewportSize().height, 0.0f, 1.0f};
                    VkRect2D   scissor  = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                    vkCmdSetViewportWithCount(cmd, 1, &viewport);
                    vkCmdSetScissorWithCount(cmd, 1, &scissor);
                    vkCmdDraw(context._currCmdBuffer, 3, 1, 0, 0);
                });
}

} // namespace Play