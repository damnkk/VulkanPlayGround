#include "LightPass.h"
#include "ShaderManager.hpp"
#include "PlayProgram.h"
#include "GBufferConfig.h"
#include "VulkanDriver.h"
#include "DeferRendering.h"
namespace Play
{

LightPass::~LightPass()
{
    ProgramPool::Instance().free(_lightPassProgram);
}

void LightPass::init()
{
    auto lightPassFragID  = ShaderManager::Instance().loadShaderFromFile("lightPassFrag", "./DefaultLightPass.frag.slang", ShaderStage::eFragment);
    auto fullScreenVertID = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    _lightPassProgram     = ProgramPool::Instance().alloc<RenderProgram>(fullScreenVertID, lightPassFragID);
    _lightPassProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();
    _lightPassProgram->psoState().rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    _lightPassProgram->psoState().rasterizationState.cullMode  = VK_CULL_MODE_NONE;
}

void LightPass::build(RDG::RDGBuilder* rdgBuilder)
{
    RDG::RDGTextureRef inputAlbedo   = rdgBuilder->getTexture("SkyBoxRT");
    RDG::RDGTextureRef inputNormal   = rdgBuilder->getTexture(GBufferConfig::Get(GBufferType::GNormal).debugName);
    RDG::RDGTextureRef inputPBR      = rdgBuilder->getTexture(GBufferConfig::Get(GBufferType::GPBR).debugName);
    RDG::RDGTextureRef inputEmissive = rdgBuilder->getTexture(GBufferConfig::Get(GBufferType::GEmissive).debugName);
    RDG::RDGTextureRef inputCustom1  = rdgBuilder->getTexture(GBufferConfig::Get(GBufferType::GCustomData).debugName);
    RDG::RDGTextureRef velocityRT    = rdgBuilder->getTexture(GBufferConfig::Get(GBufferType::GVelocity).debugName);
    RDG::RDGTextureRef depthRT       = rdgBuilder->getTexture(GBufferConfig::Get(GBufferType::GSceneDepth).debugName);
    RDG::RDGTextureRef outputLight   = rdgBuilder->createTexture("LightPassOutput")
                                         .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                         .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                         .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                                         .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                         .MipmapLevel(1)
                                         .finish();
    rdgBuilder->registTexture(outputLight);
    RDG::RenderPassNodeRef lightPass =
        rdgBuilder->createRenderPass("Lighting Pass")
            .read(0, inputAlbedo, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .read(1, inputNormal, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .read(2, inputPBR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .read(3, inputEmissive, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .read(4, inputCustom1, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .read(5, velocityRT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .read(6, depthRT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .color(0, outputLight, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .execute(
                [this](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    VkCommandBuffer   cmd              = context._currCmdBuffer;
                    PerFrameConstant* perFrameConstant = this->_lightPassProgram->getDescriptorSetManager().getPushConstantData<PerFrameConstant>();
                    perFrameConstant->cameraBufferDeviceAddress = _ownedRender->getCurrentCameraBuffer()->address;
                    this->_lightPassProgram->setPassNode(static_cast<RDG::RenderPassNode*>(node));
                    this->_lightPassProgram->bind(cmd);
                    context._pendingGfxState->bindDescriptorSet(cmd, this->_lightPassProgram);
                    VkViewport viewport = {0, 0, (float) vkDriver->getViewportSize().width, (float) vkDriver->getViewportSize().height, 0.0f, 1.0f};
                    VkRect2D   scissor  = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                    vkCmdSetViewportWithCount(cmd, 1, &viewport);
                    vkCmdSetScissorWithCount(cmd, 1, &scissor);
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                })
            .finish();
}

} // namespace Play