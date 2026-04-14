#include "GaussianDrawMeshPass.h"
#include "renderer/GaussianRenderer.h"
#include "ShaderManager.hpp"
#include "utils.hpp"

namespace Play
{

GaussianDrawMeshPass::GaussianDrawMeshPass(GaussianRenderer* renderer) : _ownedRenderer(renderer) {}

GaussianDrawMeshPass::~GaussianDrawMeshPass()
{
    // RefPtr 自动释放
}

void GaussianDrawMeshPass::init()
{
    const auto meshId = ShaderManager::Instance().loadShaderFromFile("gaussianDrawMesh", "./gaussian/gaussianDraw.mesh.slang", ShaderStage::eRayMesh,
                                                                     ShaderType::eSLANG, "main");
    const auto fragId = ShaderManager::Instance().loadShaderFromFile("gaussianDrawFrag", "./gaussian/gaussianDraw.frag.slang", ShaderStage::eFragment,
                                                                     ShaderType::eSLANG, "main");

    _meshRenderProgram = RefPtr<MeshRenderProgram>(new MeshRenderProgram());
    _meshRenderProgram->setMeshModuleID(meshId);
    _meshRenderProgram->setFragModuleID(fragId);
    _meshRenderProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();
    _meshRenderProgram->psoState().colorBlendEnables[0]                       = VK_TRUE;
    _meshRenderProgram->psoState().colorBlendEquations[0].alphaBlendOp        = VK_BLEND_OP_ADD;
    _meshRenderProgram->psoState().colorBlendEquations[0].colorBlendOp        = VK_BLEND_OP_ADD;
    _meshRenderProgram->psoState().colorBlendEquations[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _meshRenderProgram->psoState().colorBlendEquations[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _meshRenderProgram->psoState().colorBlendEquations[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    _meshRenderProgram->psoState().colorBlendEquations[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    _meshRenderProgram->psoState().rasterizationState.cullMode                = VK_CULL_MODE_NONE;

    _presentProgram              = RefPtr<RenderProgram>(new RenderProgram());
    const uint32_t presentVertId = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    const uint32_t presentFragId =
        ShaderManager::Instance().loadShaderFromFile("gaussianPresentF", "newShaders/deferRenderer/postprocess/present.frag.slang",
                                                     ShaderStage::eFragment, ShaderType::eSLANG, "main");
    _presentProgram->setVertexModuleID(presentVertId);
    _presentProgram->setFragModuleID(presentFragId);

    _presentProgram->psoState().rasterizationState.cullMode = VK_CULL_MODE_NONE;
}

void GaussianDrawMeshPass::build(RDG::RDGBuilder* rdgBuilder)
{
    RDG::RDGBufferRef  indirectBuffer  = rdgBuilder->getBuffer("indirectBuffer");
    RDG::RDGBufferRef  indicesBuffer   = rdgBuilder->getBuffer("indicesBuffer");
    RDG::RDGTextureRef colorAttachment = rdgBuilder->createTexture("meshDrawColorAttachment")
                                             .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                             .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                             .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                                             .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                             .finish();
    constexpr VkFormat meshDepthFormat = VK_FORMAT_D16_UNORM;
    RDG::RDGTextureRef depthAttachment = rdgBuilder->createTexture("meshDrawDepthAttachment")
                                             .AspectFlags(inferImageAspectFlags(meshDepthFormat, false))
                                             .UsageFlags(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                                             .Format(meshDepthFormat)
                                             .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                             .finish();

    RDG::RDGBufferRef testStorageBuffer = rdgBuilder->createBuffer("testtttt")
                                              .Location(true)
                                              .UsageFlags(VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT)
                                              .Range(VK_WHOLE_SIZE)
                                              .Size(sizeof(float4) * 2000000)
                                              .finish();

    RDG::RDGBufferRef                       sceneUniformBuffer = rdgBuilder->getBuffer("sceneUniformBuffer");
    [[maybe_unused]] RDG::RenderPassNodeRef meshDrawPass =
        rdgBuilder->createRenderPass("MeshDrawPass")
            .color(0, colorAttachment, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .depth(depthAttachment, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE)
            .storageRead(0, indirectBuffer, VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT)
            .storageRead(1, indicesBuffer, VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT)
            .read(2, sceneUniformBuffer, VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT)
            .storageWrite(3, testStorageBuffer, VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT)
            .execute(
                [this, indirectBuffer](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    VkCommandBuffer cmd = context._currCmdBuffer;

                    GaussianScene&    gaussianScene         = _ownedRenderer->getSceneManager()->getGaussianScene();
                    PerFrameConstant* pushConstant          = _meshRenderProgram->getDescriptorSetManager().getPushConstantData<PerFrameConstant>();
                    pushConstant->cameraBufferDeviceAddress = _ownedRenderer->getCurrentCameraBuffer()->address;

                    _meshRenderProgram->setPassNode(static_cast<RDG::RenderPassNode*>(node));
                    _meshRenderProgram->bind(cmd);
                    context._pendingGfxState->bindDescriptorSet(cmd, _meshRenderProgram.get());
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
            .color(0, outputTexRef, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
            .read(0, colorAttachment, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT)
            .execute(
                [this](RDG::PassNode* node, RDG::RenderContext& context)
                {
                    VkCommandBuffer cmd = context._currCmdBuffer;
                    _presentProgram->setPassNode(static_cast<RDG::RenderPassNode*>(node));
                    _presentProgram->bind(cmd);
                    context._pendingGfxState->bindDescriptorSet(cmd, _presentProgram.get());

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
