#include "PresentPass.h"
#include "PlayApp.h" // 包含 PlayElement 定义
#include "RDG/RDG.h"
namespace Play
{

PresentPass::~PresentPass() {}

void PresentPass::init()
{
    _presentProgram = std::make_unique<RenderProgram>(_element->getDevice());
    _presentProgram->setFragModuleID(ShaderManager::Instance().getShaderIdByName("postProcessf"))
        .setVertexModuleID(ShaderManager::Instance().getShaderIdByName("postProcessv"))
        .finish();
}

void PresentPass::build(RDG::RDGBuilder* rdgBuilder)
{
    // 1. 引入最终呈现的目标纹理 (通常是 UI 纹理或 Swapchain Image)
    auto presentTexRef = rdgBuilder->createTexture("presentTexture").Import(_element->getUITexture()).finish();
    auto inputTexture  = rdgBuilder->getTexture("outputTexture");
    // 2. 创建 Present Pass
    // 使用 RDG 提供的 createPresentPass 接口，将 inputTexture 写入到 presentTexRef
    rdgBuilder->createRenderPass("present")
        .program(_presentProgram.get())
        .color(0, presentTexRef, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .read(0, inputTexture, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT)
        .execute([](RDG::RenderContext& context) { vkCmdDraw(context._currCmdBuffer, 3, 1, 0, 0); });
}

} // namespace Play