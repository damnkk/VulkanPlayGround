#include "GaussianSortPass.h"
#include "core/PlayCamera.h"
#include "ShaderManager.hpp"
#include "GaussianRenderer.h"
#include "RDG/RDG.h"
#include "newshaders/gaussian/gaussianLib.h.slang"

namespace Play
{
GaussianSortPass::GaussianSortPass(GaussianRenderer* renderer)
{
    _ownedRenderer = renderer;
}
GaussianSortPass::~GaussianSortPass()
{
    ProgramPool::Instance().free(_distanceProgram);
    vrdxDestroySorter(_sorter);
}

void GaussianSortPass::init()
{
    VrdxSorterCreateInfo sorterInfo{vkDriver->getPhysicalDevice(), vkDriver->getDevice()};
    vrdxCreateSorter(&sorterInfo, &_sorter);
    vrdxGetSorterKeyValueStorageRequirements(_sorter, _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount(), &_sortRequirements);

    auto distanceComp = ShaderManager::Instance().loadShaderFromFile("DistanceComp", "./gaussian/gaussianCulling.comp.slang", ShaderStage::eCompute);
    _distanceProgram  = ProgramPool::Instance().alloc<ComputeProgram>();
    _distanceProgram->setComputeModuleID(distanceComp);
    _distanceProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();
}

void GaussianSortPass::build(RDG::RDGBuilder* rdgBuilder)
{
    RDG::RDGBufferRef distanceBuffer = rdgBuilder->createBuffer("distanceBuffer")
                                           .Location(true)
                                           .Range(VK_WHOLE_SIZE)
                                           .Size(sizeof(uint32_t) * _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount())
                                           .UsageFlags(VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT)
                                           .finish();
    rdgBuilder->registBuffer(distanceBuffer);
    RDG::RDGBufferRef indirectBuffer = rdgBuilder->createBuffer("indirectBuffer")
                                           .Location(true)
                                           .Range(VK_WHOLE_SIZE)
                                           .Size(sizeof(IndrectBuffer))
                                           .UsageFlags(VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT)
                                           .finish();
    rdgBuilder->registBuffer(indirectBuffer);
    RDG::RDGBufferRef indicesBuffer = rdgBuilder->createBuffer("indicesBuffer")
                                          .Location(true)
                                          .Range(VK_WHOLE_SIZE)
                                          .Size(sizeof(uint32_t) * _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount())
                                          .UsageFlags(VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT)
                                          .finish();
    rdgBuilder->registBuffer(indicesBuffer);

    RDG::RDGBufferRef sortStorageBuffer = rdgBuilder->createBuffer("sortStorageBuffer")
                                              .Location(true)
                                              .Range(VK_WHOLE_SIZE)
                                              .Size(_sortRequirements.size)
                                              .UsageFlags(_sortRequirements.usage)
                                              .finish();
    rdgBuilder->registBuffer(sortStorageBuffer);

    RDG::RDGBufferRef sceneUniformBuffer =
        rdgBuilder->createBuffer("sceneUniformBuffer").Import(_ownedRenderer->getSceneManager()->getGaussianScene().getSceneUniformBuffer()).finish();
    rdgBuilder->registBuffer(sceneUniformBuffer);

    RDG::ComputePassNodeRef distanceCompute =
        rdgBuilder
            ->createComputePass("DistancePass")

            .storageWrite(0, distanceBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .storageWrite(1, indirectBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .storageWrite(2, indicesBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .read(3, sceneUniformBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .execute(
                [this, indirectBuffer](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    {
                        IndrectBuffer ibuffer;
                        vkCmdUpdateBuffer(context._currCmdBuffer, indirectBuffer->getRHI()->buffer, 0, sizeof(ibuffer), (void*) &ibuffer);
                        VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                        barrier.srcAccessMask   = VK_ACCESS_TRANSFER_WRITE_BIT;
                        barrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

                        vkCmdPipelineBarrier(
                            context._currCmdBuffer, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                            0, 1, &barrier, 0, NULL, 0, NULL);
                    }

                    GaussianScene&    gaussianScene = _ownedRenderer->getSceneManager()->getGaussianScene();
                    PerFrameConstant* pushConstant  = _distanceProgram->getDescriptorSetManager().getPushConstantData<PerFrameConstant>();

                    pushConstant->cameraBufferDeviceAddress = _ownedRenderer->getCurrentCameraBuffer()->address;
                    _distanceProgram->setPassNode(static_cast<RDG::RenderPassNode*>(node));
                    _distanceProgram->bind(context._currCmdBuffer);
                    context._pendingComputeState->bindDescriptorSet(context._currCmdBuffer, _distanceProgram);
                    size_t splatCount = _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount();
                    vkCmdDispatch(context._currCmdBuffer, nvvk::getGroupCounts(splatCount, 256), 1, 1);
                    VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    barrier.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
                    vkCmdPipelineBarrier(
                        context._currCmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, 0,
                        1, &barrier, 0, NULL, 0, NULL);
                })
            .finish();
    RDG::ComputePassNodeRef sortPass =
        rdgBuilder->createComputePass("SortPass")
            .storageRead(0, indirectBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .storageRead(1, indicesBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .storageRead(2, sortStorageBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .storageRead(3, distanceBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .execute(
                [this, indirectBuffer, distanceBuffer, sortStorageBuffer, indicesBuffer](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    VkMemoryBarrier barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                    barrier.srcAccessMask   = VK_ACCESS_SHADER_WRITE_BIT;
                    barrier.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
                    vrdxCmdSortKeyValueIndirect(
                        context._currCmdBuffer, _sorter, _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount(),
                        indirectBuffer->getRHI()->buffer, offsetof(IndrectBuffer, instanceCount), distanceBuffer->getRHI()->buffer, 0,
                        indicesBuffer->getRHI()->buffer, 0, sortStorageBuffer->getRHI()->buffer, 0, VK_NULL_HANDLE, 0);
                    vkCmdPipelineBarrier(
                        context._currCmdBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, 0, 1,
                        &barrier, 0, NULL, 0, NULL);
                })
            .finish();
}

} // namespace Play