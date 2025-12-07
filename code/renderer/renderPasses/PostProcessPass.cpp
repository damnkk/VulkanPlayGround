#include "PostProcessPass.h"
#include "RDG/RDG.h"
#include "PlayProgram.h"
#include "ShaderManager.hpp"
namespace Play
{
void PostProcessPass::init()
{
    uint32_t PostProcessvId = ShaderManager::Instance().loadShaderFromFile("postProcessv", "newShaders/postProcess.vert.slang", ShaderStage::eVertex,
                                                                           ShaderType::eSLANG, "main");
    uint32_t PostProcessfId = ShaderManager::Instance().loadShaderFromFile("postProcessf", "newShaders/postProcess.frag.slang",
                                                                           ShaderStage::eFragment, ShaderType::eSLANG, "main");

    _postProgram = std::make_unique<RenderProgram>(_element->getDevice());
    _postProgram->setFragModuleID(PostProcessfId).setVertexModuleID(PostProcessvId).finish();
    _postPipelineState.rasterizationState.cullMode = VK_CULL_MODE_NONE;
}

void PostProcessPass::build(RDG::RDGBuilder* rdgBuilder)
{
    std::filesystem::path inputTexturePath = "C:\\Users\\Amin\\Desktop\\newCreated\\cici.jpg";
    Texture*              inputTex         = Texture::Create(inputTexturePath);
    auto                  colorTexId       = rdgBuilder->createTexture("inputTexture").Import(inputTex).finish();
    auto                  outputTexRef     = rdgBuilder->createTexture("outputTexture")
                            .Extent({_element->getApp()->getWindowSize().width, _element->getApp()->getWindowSize().height, 1})
                            .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                            .Format(VK_FORMAT_R8G8B8A8_UNORM)
                            .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                            .MipmapLevel(1)
                            .finish();
    rdgBuilder->registTexture(outputTexRef);

    auto pass = rdgBuilder->createRenderPass("postProcessPass")
                    .color(0, outputTexRef, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .read(0, colorTexId, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
                    .program(_postProgram.get())
                    .execute(
                        [this](RDG::RenderContext& context)
                        {
                            VkCommandBuffer cmd = context._currCmdBuffer;
                            vkCmdDraw(cmd, 3, 1, 0, 0);
                        })
                    .finish();
}

PostProcessPass::~PostProcessPass() {}

} // namespace Play