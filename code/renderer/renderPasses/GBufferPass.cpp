#include "GBufferPass.h"

#include "DeferRendering.h"
#include "PConstantType.h.slang"
#include "core/runtime/VulkanRuntime.h"
#include "resourceManagement/scene/SceneManager.h"
#include "resourceManagement/renderGraph/RDG.h"
#include "resourceManagement/vulkan/pipeline/ShaderManager.hpp"
#include "resourceManagement/vulkan/resources/VulkanResourceUtils.h"

namespace Play
{

void GBufferPass::init()
{
    const uint32_t vertexShaderID = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_VERT_SHADER_NAME);
    const uint32_t fragShaderID   = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_DEFAULT_GBUFFER_FRAG_SHADER_NAME);

    _gbufferPipeline.setShader(vertexShaderID, fragShaderID);
    _gbufferPipeline.setPushConstant<GBufferPushConstant>();
}

void GBufferPass::build(RDG::RDGBuilder* rdgBuilder)
{
    RDG::RDGTextureRef BaseColorRT = rdgBuilder->getTexture("SkyBoxRT");

    RDG::RDGTextureRef WorldNormalRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GNormal).debugName)
                                           .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                           .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                           .Format(GBufferConfig::Get(GBufferType::GNormal).format)
                                           .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                           .MipmapLevel(1)
                                           .finish();

    RDG::RDGTextureRef PBRRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GPBR).debugName)
                                   .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                   .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                   .Format(GBufferConfig::Get(GBufferType::GPBR).format)
                                   .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                   .MipmapLevel(1)
                                   .finish();

    RDG::RDGTextureRef EmissiveRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GEmissive).debugName)
                                        .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                        .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                        .Format(GBufferConfig::Get(GBufferType::GEmissive).format)
                                        .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                        .MipmapLevel(1)
                                        .finish();

    RDG::RDGTextureRef Custom1RT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GCustomData).debugName)
                                       .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                       .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                       .Format(GBufferConfig::Get(GBufferType::GCustomData).format)
                                       .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                       .MipmapLevel(1)
                                       .finish();

    RDG::RDGTextureRef VelocityRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GVelocity).debugName)
                                        .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                        .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                        .Format(GBufferConfig::Get(GBufferType::GVelocity).format)
                                        .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                        .MipmapLevel(1)
                                        .finish();

    const auto         depthFormat = GBufferConfig::Get(GBufferType::GSceneDepth).format;
    RDG::RDGTextureRef DepthRT     = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GSceneDepth).debugName)
                                         .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                         .AspectFlags(inferImageAspectFlags(depthFormat, false))
                                         .Format(depthFormat)
                                         .UsageFlags(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                         .MipmapLevel(1)
                                         .finish();

    rdgBuilder->createRenderPass("GBufferPass")
        .color(0, BaseColorRT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .color(1, WorldNormalRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .color(2, PBRRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .color(3, EmissiveRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .color(4, Custom1RT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .color(5, VelocityRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .depth(DepthRT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .execute(
            [this](RDG::PassNode*, RDG::RenderContext& context)
            {
                const auto  gpuScene = vkDriver->getSceneManager()->getGpuScene();
                const auto& buckets  = gpuScene->getDrawBuckets();
                if (buckets.empty()) return;

                VkCommandBuffer cmd      = context._currCmdBuffer;
                const auto      extent   = vkDriver->getViewportSize();
                VkViewport      viewport = {0, 0, float(extent.width), float(extent.height), 0.0f, 1.0f};
                VkRect2D        scissor  = {{0, 0}, extent};
                vkCmdSetViewportWithCount(cmd, 1, &viewport);
                vkCmdSetScissorWithCount(cmd, 1, &scissor);

                GBufferPushConstant pushConstant{};
                pushConstant.perFrameConstant.cameraBufferDeviceAddress = _ownedRender->getCurrentCameraBuffer()->address;
                pushConstant.sceneConstant.sceneAddress                 = gpuScene->getRootAddress();
                const auto pipelineBuilder                              = context.createPipelineStateBuilder();
                for (uint32_t bucketID = 0; bucketID < buckets.size(); ++bucketID)
                {
                    const auto pipeline = pipelineBuilder.build(_gbufferPipeline, buckets[bucketID].materialInstanceRef);
                    context.bindPipeline(pipeline);
                    pushConstant.sceneConstant.bucketID = bucketID;
                    context.bindPushConstant(pushConstant);
                    gpuScene->cmdDrawBucket(cmd, bucketID);
                }
            })
        .finish();
}

} // namespace Play
