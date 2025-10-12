#include "PostProcessPass.h"
#include "RDG/RDG.h"
#include "PlayProgram.h"
#include "ShaderManager.hpp"
namespace Play
{
void PostProcessPass::init()
{
    _postProgram = std::make_unique<RenderProgram>(_element->getDevice());
    _postProgram->setFragModuleID(ShaderManager::Instance().getShaderIdByName("postProcessf"))
        .setVertexModuleID(ShaderManager::Instance().getShaderIdByName("postProcessv"))
        .finish();
}

void PostProcessPass::build(RDG::RDGBuilder* rdgBuilder)
{
    std::filesystem::path inputTexturePath = "C:\\Users\\Amin\\Desktop\\DSC_0175.jpg";
    Texture*              inputTex         = Texture::Create(inputTexturePath);
    auto                  colorTexId =
        rdgBuilder->createTexture("inputTexture")
            .Import(inputTex, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0)
            .finish();
    auto outputTexRef =
        rdgBuilder->createTexture("outputTexture")
            .Extent({_element->getApp()->getWindowSize().width,
                     _element->getApp()->getWindowSize().height, 1})
            .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
            .Format(VK_FORMAT_R8G8B8A8_UNORM)
            .finish();
    auto pass =
        rdgBuilder->createRenderPass("postProcessPass")
            .color(0, outputTexRef, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE,
                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .read(0, 0, colorTexId, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                  _element->getApp()->getQueue(0).familyIndex)
            .execute(
                [this](RDG::RenderContext& context)
                {
                    VkCommandBuffer cmd = context._currCmdBuffer;
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                })
            .finish();
}

void PostProcessPass::deinit()
{
    _postProgram->deinit();
}

} // namespace Play