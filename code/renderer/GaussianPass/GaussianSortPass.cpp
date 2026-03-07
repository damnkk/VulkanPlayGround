#include "GaussianSortPass.h"
#include "core/PlayCamera.h"
#include "ShaderManager.hpp"
#include "GaussianRenderer.h"
#include "RDG/RDG.h"
#include "newshaders/gaussian/gaussianLib.h.slang"
#include "vk_radix_sort.h"
namespace Play
{
GaussianSortPass::GaussianSortPass(GaussianRenderer* renderer)
{
    _ownedRenderer = renderer;
}
GaussianSortPass::~GaussianSortPass()
{
    ProgramPool::Instance().free(_distanceProgram);
}

void GaussianSortPass::init()
{
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
                                           .UsageFlags(VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT)
                                           .finish();
    rdgBuilder->registBuffer(indirectBuffer);
    RDG::RDGBufferRef indicesBuffer = rdgBuilder->createBuffer("indicesBuffer")
                                          .Location(true)
                                          .Range(VK_WHOLE_SIZE)
                                          .Size(sizeof(uint32_t) * _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount())
                                          .UsageFlags(VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT)
                                          .finish();
    rdgBuilder->registBuffer(indicesBuffer);
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
                    GaussianPushConstant* pushConstant = _distanceProgram->getDescriptorSetManager().getPushConstantData<GaussianPushConstant>();
                    pushConstant->sceneConstant.MetaDataAddress =
                        _ownedRenderer->getSceneManager()->getGaussianScene().getSplatMetaGPUBuffer()->address;
                    pushConstant->sceneConstant.splatBufferDeviceAddress =
                        _ownedRenderer->getSceneManager()->getGaussianScene().getSplatGPUBuffer()->address;
                    pushConstant->perFrameConstant.cameraBufferDeviceAddress = _ownedRenderer->getCurrentCameraBuffer()->address;
                    _distanceProgram->setPassNode(static_cast<RDG::RenderPassNode*>(node));
                    _distanceProgram->bind(context._currCmdBuffer);
                    context._pendingComputeState->bindDescriptorSet(context._currCmdBuffer, _distanceProgram);
                    size_t splatCount = _ownedRenderer->getSceneManager()->getGaussianScene().getVertexCount();
                    vkCmdDispatch(context._currCmdBuffer, nvvk::getGroupCounts(splatCount, 256), 1, 1);
                })
            .finish();
}

} // namespace Play