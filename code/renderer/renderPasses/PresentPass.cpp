#include "PresentPass.h"
#include "PlayApp.h" // 包含 PlayElement 定义
#include "RDG/RDG.h"
#include "VulkanDriver.h"
namespace Play
{

PresentPass::~PresentPass()
{
    ProgramPool::Instance().free(_presentProgram);
}

void PresentPass::init()
{
    _presentProgram     = ProgramPool::Instance().alloc<RenderProgram>();
    uint32_t presentvID = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    uint32_t presentfID =
        ShaderManager::Instance().loadShaderFromFile("presentF", "newShaders/present.frag.slang", ShaderStage::eFragment, ShaderType::eSLANG, "main");
    _presentProgram->setFragModuleID(presentfID);
    _presentProgram->setVertexModuleID(presentvID);
    _presentProgram->psoState().rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    _presentProgram->psoState().rasterizationState.cullMode  = VK_CULL_MODE_NONE;
}

void PresentPass::build(RDG::RDGBuilder* rdgBuilder)
{
    // 1. 引入最终呈现的目标纹理 (通常是 UI 纹理或 Swapchain Image)
    auto presentTexRef = rdgBuilder->createTexture("presentTexture").Import(_element->getUITexture()).finish();
    auto inputTexture  = rdgBuilder->getTexture("outputTexture");
    // 2. 创建 Present Pass
    // 使用 RDG 提供的 createPresentPass 接口，将 inputTexture 写入到 presentTexRef
    rdgBuilder->createRenderPass("present")
        .color(0, presentTexRef, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .read(0, inputTexture, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT)
        .execute(
            [this](RDG::PassNode* passNode, RDG::RenderContext& context)
            {
                VkCommandBuffer cmd = context._currCmdBuffer;
                this->_presentProgram->setPassNode(static_cast<RDG::RenderPassNode*>(passNode));
                this->_presentProgram->bind(cmd);
                context._pendingGfxState->bindDescriptorSet(cmd, this->_presentProgram);
                VkViewport viewport = {0, 0, (float) vkDriver->getViewportSize().width, (float) vkDriver->getViewportSize().height, 0.0f, 1.0f};
                VkRect2D   scissor  = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                vkCmdSetViewportWithCount(cmd, 1, &viewport);
                vkCmdSetScissorWithCount(cmd, 1, &scissor);
                vkCmdDraw(context._currCmdBuffer, 3, 1, 0, 0);
            });
}

} // namespace Play