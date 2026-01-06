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
    uint32_t PostProcessvId = ShaderManager::Instance().loadShaderFromFile("postProcessv", "newShaders/postProcess.vert.slang", ShaderStage::eVertex,
                                                                           ShaderType::eSLANG, "main");
    uint32_t PostProcessfId = ShaderManager::Instance().loadShaderFromFile("postProcessf", "newShaders/postProcess.frag.slang",
                                                                           ShaderStage::eFragment, ShaderType::eSLANG, "main");

    _postProgram = std::make_unique<RenderProgram>(vkDriver->_device);
    _postProgram->setFragModuleID(PostProcessfId).setVertexModuleID(PostProcessvId);
    _postProgram->psoState().rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    _postProgram->psoState().rasterizationState.cullMode  = VK_CULL_MODE_NONE;
}

void PostProcessPass::build(RDG::RDGBuilder* rdgBuilder)
{
    std::filesystem::path inputTexturePath = "C:\\Users\\Amin\\Desktop\\d06f9d322937f022981ce92880703d26.jpg";
    Texture*              inputTex         = Texture::Create(inputTexturePath);
    auto                  colorTexId       = rdgBuilder->createTexture("inputTexture").Import(inputTex).finish();
    auto                  outputTexRef     = rdgBuilder->createTexture("outputTexture")
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
            .read(0, colorTexId, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .program(_postProgram.get())
            .execute(
                [ownedRender, this](RDG::PassNode* passNode, RDG::RenderContext& context)
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

                    vkCmdDraw(cmd, 3, 1, 0, 0);
                })
            .finish();
}

PostProcessPass::~PostProcessPass() {}

} // namespace Play