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
    _distanceProgram->getDescriptorSetManager().initPushConstant<GaussianPushConstant>();
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
    RDG::ComputePassNodeRef distanceCompute =
        rdgBuilder->createComputePass("DistancePass")
            .readWrite(0, distanceBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .readWrite(1, indirectBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .readWrite(2, indicesBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .execute(
                [this, indirectBuffer](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    IndrectBuffer ibuffer{};
                    vkCmdUpdateBuffer(context._currCmdBuffer, indirectBuffer->getRHI()->buffer, 0, sizeof(ibuffer), (void*) &ibuffer);
                    GaussianScene&        gaussianScene = _ownedRenderer->getSceneManager()->getGaussianScene();
                    GaussianPushConstant* pushConstant  = _distanceProgram->getDescriptorSetManager().getPushConstantData<GaussianPushConstant>();
                    pushConstant->sceneConstant.positionBufferDeviceAddress  = gaussianScene.getPositionGPUBuffer()->address;
                    pushConstant->sceneConstant.colorBufferDeviceAddress     = gaussianScene.getColorGPUBuffer()->address;
                    pushConstant->sceneConstant.opacityBufferDeviceAddress   = gaussianScene.getOpacityGPUBuffer()->address;
                    pushConstant->sceneConstant.scaleBufferDeviceAddress     = gaussianScene.getScaleGPUBuffer()->address;
                    pushConstant->sceneConstant.rotationBufferDeviceAddress  = gaussianScene.getRotationGPUBuffer()->address;
                    pushConstant->sceneConstant.positionStride               = static_cast<uint32_t>(sizeof(float3));
                    pushConstant->sceneConstant.colorStride                  = static_cast<uint32_t>(sizeof(float3));
                    pushConstant->sceneConstant.opacityStride                = static_cast<uint32_t>(sizeof(float));
                    pushConstant->sceneConstant.scaleStride                  = static_cast<uint32_t>(sizeof(float3));
                    pushConstant->sceneConstant.rotationStride               = static_cast<uint32_t>(sizeof(float4));
                    pushConstant->sceneConstant.metaDataAddress              = gaussianScene.getSplatMetaGPUBuffer()->address;
                    pushConstant->perFrameConstant.cameraBufferDeviceAddress = _ownedRenderer->getCurrentCameraBuffer()->address;
                    _distanceProgram->setPassNode(static_cast<RDG::RenderPassNode*>(node));
                    _distanceProgram->bind(context._currCmdBuffer);
                    context._pendingComputeState->bindDescriptorSet(context._currCmdBuffer, _distanceProgram);
                    size_t splatCount = _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount();
                    vkCmdDispatch(context._currCmdBuffer, nvvk::getGroupCounts(splatCount, 256), 1, 1);
                })
            .finish();
    RDG::ComputePassNodeRef sortPass =
        rdgBuilder->createComputePass("SortPass")
            .readWrite(0, indirectBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .readWrite(1, indicesBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .readWrite(2, sortStorageBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .readWrite(3, distanceBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .execute(
                [this, indirectBuffer, distanceBuffer, sortStorageBuffer, indicesBuffer](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    vrdxCmdSortKeyValueIndirect(
                        context._currCmdBuffer, _sorter, _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount(),
                        indirectBuffer->getRHI()->buffer, offsetof(IndrectBuffer, instanceCount), distanceBuffer->getRHI()->buffer, 0,
                        indicesBuffer->getRHI()->buffer, 0, sortStorageBuffer->getRHI()->buffer, 0, VK_NULL_HANDLE, 0);
                })
            .finish();
}

} // namespace Play