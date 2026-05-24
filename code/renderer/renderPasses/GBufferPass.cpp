#include "GBufferPass.h"

#include "RDG/RDG.h"
#include "VulkanDriver.h"
#include "utils.hpp"

namespace Play
{

void GBufferPass::init() {}

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
            [](RDG::PassNode* node, RDG::RenderContext& context)
            {
                (void) node;
                (void) context;
            })
        .finish();
}

} // namespace Play
