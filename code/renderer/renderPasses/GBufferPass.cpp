#include "GBufferPass.h"
#include "Material.h"
#include "RDG/RDG.h"
#include "VulkanDriver.h"
#include "MeshCollector.h"
#include "DeferRendering.h"
#include "nvutils/parallel_work.hpp"
#include "pConstantType.h.slang"
namespace Play
{

void GBufferPass::init() {}

void GBufferPass::build(RDG::RDGBuilder* rdgBuilder)
{
    RDG::RDGTextureRef BaseColorRT = rdgBuilder->getTexture("SkyBoxRT");
    rdgBuilder->registTexture(BaseColorRT);
    RDG::RDGTextureRef WorldNormalRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GNormal).debugName)
                                           .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                           .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                           .Format(GBufferConfig::Get(GBufferType::GNormal).format)
                                           .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                           .MipmapLevel(1)
                                           .finish();
    rdgBuilder->registTexture(WorldNormalRT);
    RDG::RDGTextureRef PBRRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GPBR).debugName)
                                   .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                   .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                   .Format(GBufferConfig::Get(GBufferType::GPBR).format)
                                   .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                   .MipmapLevel(1)
                                   .finish();
    rdgBuilder->registTexture(PBRRT);

    RDG::RDGTextureRef EmissiveRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GEmissive).debugName)
                                        .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                        .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                        .Format(GBufferConfig::Get(GBufferType::GEmissive).format)
                                        .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                        .MipmapLevel(1)
                                        .finish();
    rdgBuilder->registTexture(EmissiveRT);
    RDG::RDGTextureRef Custom1RT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GCustomData).debugName)
                                       .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                       .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                       .Format(GBufferConfig::Get(GBufferType::GCustomData).format)
                                       .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                       .MipmapLevel(1)
                                       .finish();
    rdgBuilder->registTexture(Custom1RT);
    RDG::RDGTextureRef VelocityRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GVelocity).debugName)
                                        .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                        .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                        .Format(GBufferConfig::Get(GBufferType::GVelocity).format)
                                        .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                        .MipmapLevel(1)
                                        .finish();
    rdgBuilder->registTexture(VelocityRT);
    RDG::RDGTextureRef DepthRT = rdgBuilder->createTexture(GBufferConfig::Get(GBufferType::GSceneDepth).debugName)
                                     .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                     .AspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT)
                                     .Format(GBufferConfig::Get(GBufferType::GSceneDepth).format)
                                     .UsageFlags(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                     .MipmapLevel(1)
                                     .finish();
    rdgBuilder->registTexture(DepthRT);

    RDG::RenderPassNodeRef gBufferPass =
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
                [this](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    // cast renderpass node,and collect renderPass info
                    RDG::RenderPassNodeRef renderPassNode   = dynamic_cast<RDG::RenderPassNodeRef>(node);
                    SceneManager*          sceneManager     = _ownedRender->getSceneManager();
                    RenderPassConfig&      renderPassConfig = renderPassNode->getRenderPass()->getConfig();
                    std::vector<VkFormat>  colorFormats;
                    for (const auto& colorAttachment : renderPassConfig.colorAttachments)
                    {
                        colorFormats.push_back(colorAttachment.format);
                    }

                    VkCommandBufferInheritanceRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO};
                    renderingInfo.colorAttachmentCount    = static_cast<uint32_t>(colorFormats.size());
                    renderingInfo.pColorAttachmentFormats = colorFormats.data();
                    renderingInfo.depthAttachmentFormat =
                        renderPassConfig.depthAttachment.has_value() ? renderPassConfig.depthAttachment->format : VK_FORMAT_UNDEFINED;
                    renderingInfo.stencilAttachmentFormat =
                        renderPassConfig.stencilAttachment.has_value() ? renderPassConfig.stencilAttachment->format : VK_FORMAT_UNDEFINED;
                    ;

                    renderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

                    VkCommandBufferInheritanceInfo inheritanceInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
                    inheritanceInfo.pNext = &renderingInfo;

                    // collect mesh batch
                    MeshCollector           meshCollector(this->_ownedRender);
                    std::vector<MeshBatch>& meshBatches = meshCollector.collectMeshBatches();
                    {
                        std::unordered_set<uint64_t> processedMaterialID;
                        for (auto& batch : meshBatches)
                        {
                            RenderScene& gpuScene = sceneManager->getVkScene()[batch.sceneID];
                            Material*    mat      = gpuScene.getDefaultMaterials()[batch.materialID];
                            mat->getProgram()->setPassNode(node);
                        }
                    }
                    // dispatch num
                    uint32_t batchNum = (static_cast<uint32_t>(meshBatches.size()) + MAX_SUB_RENDER_THREAD - 1) / MAX_SUB_RENDER_THREAD;
                    std::array<VkCommandBuffer, MAX_SUB_RENDER_THREAD> cmdBuffers = {VK_NULL_HANDLE};
                    BS::thread_pool&                                   threadPool = nvutils::get_thread_pool();
                    BS::multi_future<void>                             res        = threadPool.submit_sequence<uint32_t>(
                        0, MAX_SUB_RENDER_THREAD,
                        [&](uint32_t threadIndex)
                        {
                            uint32_t beginIndex = threadIndex * batchNum;
                            if (beginIndex >= meshBatches.size()) return;

                            VkCommandBuffer subCmdBuffer = context._frameData->workerGraphicsPools.getCommandBuffer(threadIndex);
                            cmdBuffers[threadIndex] = subCmdBuffer;

                            VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
                            beginInfo.pInheritanceInfo = &inheritanceInfo;

                            NVVK_CHECK(vkBeginCommandBuffer(subCmdBuffer, &beginInfo));

                            // Simple loop for now, optimize later
                            uint32_t endBatchIndex = std::min((uint32_t) meshBatches.size(), beginIndex + batchNum);
                            for (uint32_t i = beginIndex; i < endBatchIndex; ++i)
                            {
                                MeshBatch&   batch    = meshBatches[i];
                                RenderScene& gpuScene = sceneManager->getVkScene()[batch.sceneID];
                                PlayProgram* program  = gpuScene.getDefaultMaterials()[batch.materialID]->getProgram();
                                VkViewport   viewport = {
                                    0, 0, (float) vkDriver->getViewportSize().width, (float) vkDriver->getViewportSize().height, 0.0f, 1.0f};
                                VkRect2D scissor = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                                vkCmdSetViewportWithCount(subCmdBuffer, 1, &viewport);
                                vkCmdSetScissorWithCount(subCmdBuffer, 1, &scissor);
                                vkCmdSetDepthWriteEnable(subCmdBuffer, VK_FALSE);
                                vkCmdSetDepthTestEnable(subCmdBuffer, VK_FALSE);
                                program->bind(subCmdBuffer);
                                context._pendingGfxState->bindDescriptorSet(subCmdBuffer, program);
                                for (int i = 0; i < batch.renderNodeIDs.size(); ++i)
                                {
                                    nvvkgltf::Scene&                 scene = sceneManager->getCpuScene()[batch.sceneID];
                                    RenderScene&                     gpuScene = sceneManager->getVkScene()[batch.sceneID];
                                    const nvvkgltf::RenderNode&      renderNode = scene.getRenderNodes()[batch.renderNodeIDs[i]];
                                    const nvvkgltf::RenderPrimitive& renderPrimitive = scene.getRenderPrimitives()[renderNode.renderPrimID];
                                    auto&                            indexBuffer = gpuScene.indices()[renderNode.renderPrimID];
                                    int                              indexCount = renderPrimitive.indexCount;
                                    GBufferPushConstant* constant = program->getDescriptorSetManager().getPushConstantData<GBufferPushConstant>();
                                    constant->perFrameConstant.cameraBufferDeviceAddress = _ownedRender->getCurrentCameraBuffer()->address;
                                    constant->sceneConstant.sceneDescAddress = sceneManager->getVkScene()[batch.sceneID].sceneDesc().address;
                                    constant->sceneConstant.renderNodeId  = batch.renderNodeIDs[i];
                                    constant->sceneConstant.textureOffset = gpuScene.getTextureOffset();
                                    program->getDescriptorSetManager().pushConstantRanges(subCmdBuffer);
                                    vkCmdBindIndexBuffer2(subCmdBuffer, indexBuffer.buffer, 0, indexBuffer.bufferSize,
                                                                                             VkIndexType::VK_INDEX_TYPE_UINT32);
                                    vkCmdDrawIndexed(subCmdBuffer, indexCount, 1, 0, 0, 0);
                                }
                            }

                            NVVK_CHECK(vkEndCommandBuffer(subCmdBuffer));
                        });
                    res.wait();

                    // Filter out null buffers if thread count > batch count
                    std::vector<VkCommandBuffer> validCmdBuffers;
                    validCmdBuffers.reserve(MAX_SUB_RENDER_THREAD);
                    for (auto cb : cmdBuffers)
                    {
                        if (cb != VK_NULL_HANDLE) validCmdBuffers.push_back(cb);
                    }

                    if (!validCmdBuffers.empty())
                    {
                        vkCmdExecuteCommands(context._currCmdBuffer, static_cast<uint32_t>(validCmdBuffers.size()), validCmdBuffers.data());
                    }
                })
            .finish();
    gBufferPass->setMultiThreadRecordingState(true);
}

} // namespace Play