#include "VolumeSkyPass.h"
#include "VulkanDriver.h"
#include "ShaderManager.hpp"
#include "RDG/RDG.h"
#include "DeferRendering.h"
#include <imgui/imgui.h>

namespace
{
constexpr uint32_t kTransmittanceLutWidth  = 256;
constexpr uint32_t kTransmittanceLutHeight = 64;
constexpr uint32_t kTransmittanceGroupSize = 8;
} // namespace

namespace Play
{

void VolumeSkyPass::AtmosControler::onGUI()
{
    bool changed = false;
    // if (ImGui::Begin("Atmosphere Profile"))
    // {
    //     changed |= ImGui::DragFloat("Sea Level", &_cpuData.seaLevel, 1.0f, 0.0f, 100000.0f, "%.1f");
    //     changed |= ImGui::DragFloat("Planet Radius", &_cpuData.planetRadius, 100.0f, 0.0f, 100000000.0f, "%.1f");
    //     changed |= ImGui::DragFloat("Atmosphere Height", &_cpuData.atmosphereHeight, 10.0f, 0.0f, 1000000.0f, "%.1f");
    //     changed |= ImGui::DragFloat("Sun Intensity", &_cpuData.sunLightIntensity, 0.1f, 0.0f, 100000.0f, "%.3f");
    //     changed |= ImGui::ColorEdit3("Sun Color", &_cpuData.sunColor.x);
    //     changed |= ImGui::SliderFloat("Sun Disk Angle", &_cpuData.sunDiskAngle, 0.0f, 90.0f, "%.3f");
    //     changed |= ImGui::SliderFloat("Sun Azimuth", &_cpuData.sunSit.x, 0.0f, 360.0f, "%.1f deg");
    //     changed |= ImGui::SliderFloat("Sun Elevation", &_cpuData.sunSit.y, -89.0f, 89.0f, "%.1f deg");
    //     changed |= ImGui::DragFloat("Rayleigh Scale", &_cpuData.rayleighScatteringScale, 0.01f, 0.0f, 100.0f, "%.3f");
    //     changed |= ImGui::DragFloat("Rayleigh Height", &_cpuData.rayleighScatteringScalarHeight, 10.0f, 1.0f, 1000000.0f, "%.1f");
    //     changed |= ImGui::DragFloat("Mie Scale", &_cpuData.mieScatteringScale, 0.01f, 0.0f, 100.0f, "%.3f");
    //     changed |= ImGui::DragFloat("Mie Height", &_cpuData.mieScatteringScalarHeight, 10.0f, 1.0f, 1000000.0f, "%.1f");
    //     changed |= ImGui::SliderFloat("Mie Anisotropy", &_cpuData.mieAnisotropy, 0.0f, 0.999f, "%.3f");
    //     changed |= ImGui::DragFloat("Ozone Scale", &_cpuData.OZoneAbsorptionScale, 0.01f, 0.0f, 100.0f, "%.3f");
    //     changed |= ImGui::DragFloat("Ozone Center Height", &_cpuData.OZoneLevelCenterHeight, 10.0f, 0.0f, 1000000.0f, "%.1f");
    //     changed |= ImGui::DragFloat("Ozone Width", &_cpuData.OZoneLevelWidth, 10.0f, 1.0f, 1000000.0f, "%.1f");
    // }
    // ImGui::End();

    if (changed) flushToGPU();
}

VolumeSkyPass::~VolumeSkyPass()
{
    // RefPtr 自动释放
}

void VolumeSkyPass::init()
{
    auto skyBoxvId = ShaderManager::Instance().getShaderIdByName(BuiltinShaders::BUILTIN_FULL_SCREEN_QUAD_VERT_SHADER_NAME);
    auto skyBoxfId =
        ShaderManager::Instance().loadShaderFromFile("skyBoxFragment", "newShaders/deferRenderer/atmosphere/skyBoxProgram.frag.slang",
                                                     ShaderStage::eFragment);
    auto transmittanceComp = ShaderManager::Instance().loadShaderFromFile("transmittanceLutComp",
                                                                          "newShaders/deferRenderer/atmosphere/transmittanceLut.comp.slang",
                                                                          ShaderStage::eCompute);

    _transmittanceLutProgram = RefPtr<ComputeProgram>(new ComputeProgram());
    _transmittanceLutProgram->setComputeModuleID(transmittanceComp);

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

    _skyAtmosControler.flushToGPU();
}

void VolumeSkyPass::build(RDG::RDGBuilder* rdgBuilder)
{
    DeferRenderer* ownedRender         = static_cast<DeferRenderer*>(_ownedRender);
    auto           transmittanceLutRef = rdgBuilder->createTexture("TransmittanceLut").Import(_transmittanceLut.get()).finish();
    auto           SkyBoxRT            = rdgBuilder->createTexture("SkyBoxRT")
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
                [this](RDG::PassNode* passNode, RDG::RenderContext& context)
                {
                    _transmittanceLutProgram->setPassNode(passNode);
                    _transmittanceLutProgram->bind(context._currCmdBuffer);
                    context._pendingComputeState->bindDescriptorSet(context._currCmdBuffer, _transmittanceLutProgram.get());
                    vkCmdDispatch(context._currCmdBuffer, (kTransmittanceLutWidth + kTransmittanceGroupSize - 1) / kTransmittanceGroupSize,
                                  (kTransmittanceLutHeight + kTransmittanceGroupSize - 1) / kTransmittanceGroupSize, 1);
                })
            .finish();

    auto skyBoxPass =
        rdgBuilder->createRenderPass("skyBoxPass")
            .color(0, SkyBoxRT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .read(0, transmittanceLutRef, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
            .read(1, atmosBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
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

void VolumeSkyPass::onGUI()
{
    _skyAtmosControler.onGUI();
}

} // namespace Play
