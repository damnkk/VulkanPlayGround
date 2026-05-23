#include "VolumeSkyPass.h"
#include "VulkanDriver.h"
#include "ShaderManager.hpp"
#include "RDG/RDG.h"
#include "DeferRendering.h"
#include "editor/EditorRegistry.h"

namespace
{
constexpr uint32_t         kTransmittanceLutWidth     = 256;
constexpr uint32_t         kTransmittanceLutHeight    = 64;
constexpr uint32_t         kTransmittanceGroupSize    = 8;
constexpr uint32_t         kMultiScatteringLutWidth   = 32;
constexpr uint32_t         kMultiScatteringLutHeight  = 32;
constexpr uint32_t         kSkyViewLutWidth           = 192;
constexpr uint32_t         kSkyViewLutHeight          = 108;
constexpr uint32_t         kSkyViewGroupSize          = 8;
} // namespace

namespace Play
{

VolumeSkyPass::~VolumeSkyPass()
{
    // RefPtr 自动释放
}

void VolumeSkyPass::init()
{
    auto skyBoxvId = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    auto skyBoxfId = ShaderManager::Instance().loadShaderFromFile("skyBoxFragment", "newShaders/deferRenderer/atmosphere/skyBoxProgram.frag.slang",
                                                                  ShaderStage::eFragment);
    auto transmittanceComp = ShaderManager::Instance().loadShaderFromFile(
        "transmittanceLutComp", "newShaders/deferRenderer/atmosphere/transmittanceLut.comp.slang", ShaderStage::eCompute);
    auto multiScatteringComp = ShaderManager::Instance().loadShaderFromFile(
        "multiScatteringLutComp", "newShaders/deferRenderer/atmosphere/multiScatteringLut.comp.slang", ShaderStage::eCompute);
    auto skyViewComp = ShaderManager::Instance().loadShaderFromFile("skyViewLutComp", "newShaders/deferRenderer/atmosphere/skyViewLut.comp.slang",
                                                                    ShaderStage::eCompute);

    _transmittanceLutProgram = RefPtr<ComputeProgram>(new ComputeProgram());
    _transmittanceLutProgram->setComputeModuleID(transmittanceComp);
    _transmittanceLutProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();

    _multiScatteringLutProgram = RefPtr<ComputeProgram>(new ComputeProgram());
    _multiScatteringLutProgram->setComputeModuleID(multiScatteringComp);
    _multiScatteringLutProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();

    _skyViewLutProgram = RefPtr<ComputeProgram>(new ComputeProgram());
    _skyViewLutProgram->setComputeModuleID(skyViewComp);
    _skyViewLutProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();

    _skyBoxProgram = RefPtr<RenderProgram>(new RenderProgram());
    _skyBoxProgram->setFragModuleID(skyBoxfId);
    _skyBoxProgram->setVertexModuleID(skyBoxvId);
    _skyBoxProgram->getDescriptorSetManager().initPushConstant<PerFrameConstant>();
    _skyBoxProgram->psoState().rasterizationState.frontFace       = VK_FRONT_FACE_CLOCKWISE;
    _skyBoxProgram->psoState().rasterizationState.cullMode        = VK_CULL_MODE_NONE;
    _skyBoxProgram->psoState().depthStencilState.depthWriteEnable = VK_FALSE;
    _skyBoxProgram->psoState().depthStencilState.depthTestEnable  = VK_FALSE;

    _transmittanceLut              = RefPtr<Texture>(new Texture(kTransmittanceLutWidth, kTransmittanceLutHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                                 VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_GENERAL));
    _transmittanceLut->DebugName() = "TransmittanceLut";
    _multiScatteringLut              = RefPtr<Texture>(new Texture(kMultiScatteringLutWidth, kMultiScatteringLutHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                                   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_GENERAL));
    _multiScatteringLut->DebugName() = "MultiScatteringLut";
    _skyViewLut                      = RefPtr<Texture>(new Texture(kSkyViewLutWidth, kSkyViewLutHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                                   VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_GENERAL));
    _skyViewLut->DebugName()         = "SkyViewLut";

    _skyAtmosControler.flushToGPU();
    vkDriver->getEditorRegistry().registerWritable<AtmosphereParameters>("Atmosphere", _skyAtmosControler, editor::EditorRenderMode::Defer);
}

void VolumeSkyPass::build(RDG::RDGBuilder* rdgBuilder)
{
    DeferRenderer* ownedRender           = static_cast<DeferRenderer*>(_ownedRender);
    auto           transmittanceLutRef   = rdgBuilder->createTexture("TransmittanceLut").Import(_transmittanceLut.get()).finish();
    auto           multiScatteringLutRef = rdgBuilder->createTexture("MultiScatteringLut").Import(_multiScatteringLut.get()).finish();
    auto           skyViewLutRef         = rdgBuilder->createTexture("SkyViewLut").Import(_skyViewLut.get()).finish();
    auto           SkyBoxRT              = rdgBuilder->createTexture("SkyBoxRT")
                        .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                        .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                        .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                        .UsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                        .MipmapLevel(1)
                        .finish();
    auto atmosBuffer = rdgBuilder->createBuffer("atmosBuffer").Import(_skyAtmosControler.getGPUBuffer()).finish();

    auto transmisstanceLutPass =
        rdgBuilder->createComputePass("TransmittanceLutPass")
            .storageWrite(0, transmittanceLutRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .read(1, atmosBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .execute(
                [this, ownedRender](RDG::PassNode* passNode, RDG::RenderContext& context)
                {
                    PerFrameConstant* perFrameConstant =
                        this->_transmittanceLutProgram->getDescriptorSetManager().getPushConstantData<PerFrameConstant>();
                    perFrameConstant->cameraBufferDeviceAddress = ownedRender->getCurrentCameraBuffer()->address;
                    _transmittanceLutProgram->setPassNode(passNode);
                    _transmittanceLutProgram->bind(context._currCmdBuffer);
                    context._pendingComputeState->bindDescriptorSet(context._currCmdBuffer, _transmittanceLutProgram.get());
                    vkCmdDispatch(context._currCmdBuffer, (kTransmittanceLutWidth + kTransmittanceGroupSize - 1) / kTransmittanceGroupSize,
                                  (kTransmittanceLutHeight + kTransmittanceGroupSize - 1) / kTransmittanceGroupSize, 1);
                })
            .finish();

    auto multiScatteringLutPass =
        rdgBuilder->createComputePass("MultiScatteringLutPass")
            .storageWrite(0, multiScatteringLutRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .read(1, transmittanceLutRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .read(2, atmosBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .execute(
                [this, ownedRender](RDG::PassNode* passNode, RDG::RenderContext& context)
                {
                    PerFrameConstant* perFrameConstant =
                        this->_multiScatteringLutProgram->getDescriptorSetManager().getPushConstantData<PerFrameConstant>();
                    perFrameConstant->cameraBufferDeviceAddress = ownedRender->getCurrentCameraBuffer()->address;
                    _multiScatteringLutProgram->setPassNode(passNode);
                    _multiScatteringLutProgram->bind(context._currCmdBuffer);
                    context._pendingComputeState->bindDescriptorSet(context._currCmdBuffer, _multiScatteringLutProgram.get());
                    vkCmdDispatch(context._currCmdBuffer, kMultiScatteringLutWidth, kMultiScatteringLutHeight, 1);
                })
            .finish();

    auto skyViewLutPass =
        rdgBuilder->createComputePass("SkyViewLutPass")
            .storageWrite(0, skyViewLutRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .read(1, transmittanceLutRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .read(2, multiScatteringLutRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .read(3, atmosBuffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
            .execute(
                [this, ownedRender](RDG::PassNode* passNode, RDG::RenderContext& context)
                {
                    PerFrameConstant* perFrameConstant =
                        this->_skyViewLutProgram->getDescriptorSetManager().getPushConstantData<PerFrameConstant>();
                    perFrameConstant->cameraBufferDeviceAddress = ownedRender->getCurrentCameraBuffer()->address;
                    _skyViewLutProgram->setPassNode(passNode);
                    _skyViewLutProgram->bind(context._currCmdBuffer);
                    context._pendingComputeState->bindDescriptorSet(context._currCmdBuffer, _skyViewLutProgram.get());
                    vkCmdDispatch(context._currCmdBuffer, (kSkyViewLutWidth + kSkyViewGroupSize - 1) / kSkyViewGroupSize,
                                  (kSkyViewLutHeight + kSkyViewGroupSize - 1) / kSkyViewGroupSize, 1);
                })
            .finish();

    auto skyBoxPass =
        rdgBuilder->createRenderPass("skyBoxPass")
            .color(0, SkyBoxRT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .read(0, skyViewLutRef, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .read(1,atmosBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .execute(
                [this, ownedRender](RDG::PassNode* passNode, RDG::RenderContext& context)
                {
                    VkCommandBuffer   cmd              = context._currCmdBuffer;
                    PerFrameConstant* perFrameConstant = this->_skyBoxProgram->getDescriptorSetManager().getPushConstantData<PerFrameConstant>();
                    perFrameConstant->cameraBufferDeviceAddress = ownedRender->getCurrentCameraBuffer()->address;
                    this->_skyBoxProgram->setPassNode(static_cast<RDG::RenderPassNode*>(passNode));
                    this->_skyBoxProgram->bind(cmd);
                    context._pendingGfxState->bindDescriptorSet(cmd, this->_skyBoxProgram.get());
                    VkViewport viewport = {0, 0, (float) vkDriver->getViewportSize().width, (float) vkDriver->getViewportSize().height, 0.0f, 1.0f};
                    VkRect2D   scissor  = {{0, 0}, {vkDriver->getViewportSize().width, vkDriver->getViewportSize().height}};
                    vkCmdSetViewportWithCount(cmd, 1, &viewport);
                    vkCmdSetScissorWithCount(cmd, 1, &scissor);
                    vkCmdDraw(cmd, 3, 1, 0, 0);
                });
}

} // namespace Play
