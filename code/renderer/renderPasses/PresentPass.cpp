#include "PresentPass.h"
#include "RDG/RDG.h"
#include "ShaderManager.hpp"
#include "core/runtime/VulkanRuntime.h"
namespace Play
{

PresentPass::~PresentPass() = default;

void PresentPass::init()
{
    uint32_t presentvID = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    uint32_t presentfID = ShaderManager::Instance().loadShaderFromFile("presentF", "newShaders/deferRenderer/postprocess/present.frag.slang",
                                                                       ShaderStage::eFragment, ShaderType::eSLANG, "main");
    _presentPipeline.setShader(presentvID, presentfID);
    _presentPipeline.psoState.rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    _presentPipeline.psoState.rasterizationState.cullMode  = VK_CULL_MODE_NONE;
}

void PresentPass::build(RDG::RDGBuilder* rdgBuilder)
{
    auto presentTexRef = rdgBuilder->createTexture(PRESENT_TEXTURE_NAME).finish();
    auto inputTexture  = rdgBuilder->getTexture("outputTexture");
    rdgBuilder->createRenderPass("present")
        .color(0, presentTexRef, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        .read(0, inputTexture, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT)
        .execute(
            [this](RDG::PassNode* passNode, RDG::RenderContext& context)
            {
                VkCommandBuffer cmd = context._currCmdBuffer;
                context.bindPipeline(this->_presentPipeline);
                VkViewport viewport = {0, 0, (float) vkDriver->getViewportSize().width, (float) vkDriver->getViewportSize().height, 0.0f, 1.0f};
                VkRect2D   scissor  = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                vkCmdSetViewportWithCount(cmd, 1, &viewport);
                vkCmdSetScissorWithCount(cmd, 1, &scissor);
                vkCmdDraw(context._currCmdBuffer, 3, 1, 0, 0);
            });
}

} // namespace Play
