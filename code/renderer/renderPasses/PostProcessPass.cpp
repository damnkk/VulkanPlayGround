#include "PostProcessPass.h"
#include "DeferRendering.h"
#include "RDG/RDG.h"
#include "PlayProgram.h"
#include "ShaderManager.hpp"
#include "VulkanDriver.h"
#include "resourceManagement/PconstantType.h.slang"
namespace Play
{
void PostProcessPass::init()
{
    uint32_t PostProcessvId = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    uint32_t PostProcessfId = ShaderManager::Instance().loadShaderFromFile("postProcessf", "newShaders/postProcess.frag.slang",
                                                                           ShaderStage::eFragment, ShaderType::eSLANG, "main");

    _postProgram = ProgramPool::Instance().alloc<RenderProgram>();
    _postProgram->setFragModuleID(PostProcessfId).setVertexModuleID(PostProcessvId);
    _postProgram->psoState().rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    _postProgram->psoState().rasterizationState.cullMode  = VK_CULL_MODE_NONE;
}

void PostProcessPass::build(RDG::RDGBuilder* rdgBuilder)
{
    auto inputTexture = rdgBuilder->getTexture("SkyBoxRT");
    auto outputTexRef = rdgBuilder->createTexture("outputTexture")
                            .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                            .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                            .Format(VK_FORMAT_R8G8B8A8_UNORM)
                            .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                            .MipmapLevel(1)
                            .finish();
    rdgBuilder->registTexture(outputTexRef);
    DeferRenderer* ownedRender = static_cast<DeferRenderer*>(_ownedRender);
    auto           pass =
        rdgBuilder->createRenderPass("postProcessPass")
            .color(0, outputTexRef, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .read(0, inputTexture, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .execute(
                [ownedRender, this](RDG::PassNode* passNode, RDG::RenderContext& context)
                {
                    VkCommandBuffer  cmd = context._currCmdBuffer;
                    PerFrameConstant perFrameConstant;
                    perFrameConstant.cameraBufferDeviceAddress = ownedRender->getCurrentCameraBuffer()->address;
                    this->_postProgram->setPassNode(static_cast<RDG::RenderPassNode*>(passNode));
                    this->_postProgram->getDescriptorSetManager().setConstantRange(perFrameConstant);
                    this->_postProgram->bind(cmd);
                    context._pendingGfxState->bindDescriptorSet(cmd, this->_postProgram);
                    VkViewport viewport = {0, 0, (float) vkDriver->getViewportSize().width, (float) vkDriver->getViewportSize().height, 0.0f, 1.0f};
                    VkRect2D   scissor  = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                    vkCmdSetViewportWithCount(cmd, 1, &viewport);
                    vkCmdSetScissorWithCount(cmd, 1, &scissor);
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                })
            .finish();
}

PostProcessPass::~PostProcessPass() {}

} // namespace Play