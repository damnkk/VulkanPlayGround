#include "GaussianDrawMeshPass.h"
#include "renderer/GaussianRenderer.h"
#include "ShaderManager.hpp"

namespace Play
{

GaussianDrawMeshPass::GaussianDrawMeshPass(GaussianRenderer* renderer) : _ownedRenderer(renderer) {}

GaussianDrawMeshPass::~GaussianDrawMeshPass()
{
    ProgramPool::Instance().free(_meshRenderProgram);
    ProgramPool::Instance().free(_presentProgram);
}

void GaussianDrawMeshPass::init()
{
    const auto meshId = ShaderManager::Instance().loadShaderFromFile("gaussianDrawMesh", "./gaussian/gaussianDraw.mesh.slang", ShaderStage::eRayMesh,
                                                                     ShaderType::eSLANG, "main");
    const auto fragId = ShaderManager::Instance().loadShaderFromFile("gaussianDrawFrag", "./gaussian/gaussianDraw.frag.slang", ShaderStage::eFragment,
                                                                     ShaderType::eSLANG, "main");

    _meshRenderProgram = ProgramPool::Instance().alloc<MeshRenderProgram>();
    _meshRenderProgram->setMeshModuleID(meshId);
    _meshRenderProgram->setFragModuleID(fragId);
    _meshRenderProgram->getDescriptorSetManager().initPushConstant<GaussianPushConstant>();

    _presentProgram              = ProgramPool::Instance().alloc<RenderProgram>();
    const uint32_t presentVertId = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    const uint32_t presentFragId = ShaderManager::Instance().loadShaderFromFile("gaussianPresentF", "newShaders/present.frag.slang",
                                                                                ShaderStage::eFragment, ShaderType::eSLANG, "main");
    _presentProgram->setVertexModuleID(presentVertId);
    _presentProgram->setFragModuleID(presentFragId);
    _presentProgram->psoState().rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    _presentProgram->psoState().rasterizationState.cullMode  = VK_CULL_MODE_NONE;
}

void GaussianDrawMeshPass::build(RDG::RDGBuilder* rdgBuilder)
{
    RDG::RDGBufferRef  indirectBuffer  = rdgBuilder->getBuffer("indirectBuffer");
    RDG::RDGTextureRef colorAttachment = rdgBuilder->createTexture("meshDrawColorAttachment")
                                             .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                             .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                             .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                                             .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                             .finish();
    rdgBuilder->registTexture(colorAttachment);
    RDG::RDGTextureRef depthAttachment = rdgBuilder->createTexture("meshDrawDepthAttachment")
                                             .AspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT)
                                             .UsageFlags(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                                             .Format(VK_FORMAT_D16_UNORM)
                                             .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                             .finish();
    rdgBuilder->registTexture(depthAttachment);
    [[maybe_unused]] RDG::RenderPassNodeRef meshDrawPass =
        rdgBuilder->createRenderPass("MeshDrawPass")
            .color(0, colorAttachment, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .depth(depthAttachment, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
            .readWrite(0, indirectBuffer, VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT)
            .execute(
                [this, indirectBuffer](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    VkCommandBuffer cmd = context._currCmdBuffer;
                    _meshRenderProgram->setPassNode(static_cast<RDG::RenderPassNode*>(node));
                    _meshRenderProgram->bind(cmd);
                    context._pendingGfxState->bindDescriptorSet(cmd, _meshRenderProgram);
                    VkViewport viewport = {
                        0,    0,    static_cast<float>(vkDriver->getViewportSize().width), static_cast<float>(vkDriver->getViewportSize().height),
                        0.0f, 1.0f,
                    };
                    VkRect2D scissor = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                    vkCmdSetViewportWithCount(cmd, 1, &viewport);
                    vkCmdSetScissorWithCount(cmd, 1, &scissor);
                    vkCmdDrawMeshTasksIndirectEXT(cmd, indirectBuffer->getRHI()->buffer, offsetof(IndrectBuffer, groupCountX), 1,
                                                  sizeof(VkDrawMeshTasksIndirectCommandEXT));
                })
            .finish();

    auto outputTexRef = rdgBuilder->createTexture("outputTexture").Import(_ownedRenderer->getOutputTexture()).finish();
    [[maybe_unused]] RDG::RenderPassNodeRef presentPass =
        rdgBuilder->createRenderPass("PresentPass")
            .color(0, outputTexRef, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE)
            .read(0, colorAttachment, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT)
            .execute(
                [this](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    VkCommandBuffer cmd = context._currCmdBuffer;
                    _presentProgram->setPassNode(static_cast<RDG::RenderPassNode*>(node));
                    _presentProgram->bind(cmd);
                    context._pendingGfxState->bindDescriptorSet(cmd, _presentProgram);

                    VkViewport viewport = {
                        0,    0,    static_cast<float>(vkDriver->getViewportSize().width), static_cast<float>(vkDriver->getViewportSize().height),
                        0.0f, 1.0f,
                    };
                    VkRect2D scissor = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                    vkCmdSetViewportWithCount(cmd, 1, &viewport);
                    vkCmdSetScissorWithCount(cmd, 1, &scissor);
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                })
            .finish();
}

} // namespace Play
