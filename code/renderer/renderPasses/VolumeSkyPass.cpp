#include "VolumeSkyPass.h"
#include "VulkanDriver.h"
#include "ShaderManager.hpp"
#include "RDG/RDG.h"
#include "DeferRendering.h"
#include <imgui/imgui.h>

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
const AtmosphereParameters kDefaultAtmosphereParameters{};
} // namespace

namespace Play
{

void VolumeSkyPass::AtmosControler::onGUI()
{
    bool changed = false;
    if (ImGui::Begin("Atmosphere Profile"))
    {
        if (ImGui::Button("Reset To Defaults"))
        {
            _cpuData = kDefaultAtmosphereParameters;
            changed  = true;
        }

        ImGui::TextUnformatted("Geometry");
        ImGui::Separator();
        changed |= ImGui::DragFloat("Bottom Radius (km)", &_cpuData.BottomRadius, 1.0f, 1000.0f, 10000.0f, "%.2f");
        changed |= ImGui::DragFloat("Top Radius (km)", &_cpuData.TopRadius, 1.0f, 1000.0f, 10000.0f, "%.2f");

        ImGui::TextUnformatted("Rayleigh");
        ImGui::Separator();
        changed |= ImGui::DragFloat("Rayleigh Density Exp Scale", &_cpuData.RayleighDensityExpScale, 0.001f, -2.0f, 0.0f, "%.4f");
        changed |= ImGui::DragFloat3("Rayleigh Scattering", &_cpuData.RayleighScattering.x, 0.0001f, 0.0f, 0.1f, "%.6f");

        ImGui::TextUnformatted("Mie");
        ImGui::Separator();
        changed |= ImGui::DragFloat("Mie Density Exp Scale", &_cpuData.MieDensityExpScale, 0.001f, -2.0f, 0.0f, "%.4f");
        changed |= ImGui::DragFloat3("Mie Scattering", &_cpuData.MieScattering.x, 0.0001f, 0.0f, 0.1f, "%.6f");
        changed |= ImGui::DragFloat3("Mie Extinction", &_cpuData.MieExtinction.x, 0.0001f, 0.0f, 0.1f, "%.6f");
        changed |= ImGui::DragFloat3("Mie Absorption", &_cpuData.MieAbsorption.x, 0.0001f, 0.0f, 0.1f, "%.6f");
        changed |= ImGui::SliderFloat("Mie Phase G", &_cpuData.MiePhaseG, 0.0f, 0.999f, "%.3f");

        ImGui::TextUnformatted("Absorption");
        ImGui::Separator();
        changed |= ImGui::DragFloat("Absorption Layer Width (km)", &_cpuData.AbsorptionDensity0LayerWidth, 0.1f, 0.0f, 100.0f, "%.3f");
        changed |= ImGui::DragFloat("Absorption Density0 Constant", &_cpuData.AbsorptionDensity0ConstantTerm, 0.001f, -10.0f, 10.0f, "%.4f");
        changed |= ImGui::DragFloat("Absorption Density0 Linear", &_cpuData.AbsorptionDensity0LinearTerm, 0.001f, -1.0f, 1.0f, "%.4f");
        changed |= ImGui::DragFloat("Absorption Density1 Constant", &_cpuData.AbsorptionDensity1ConstantTerm, 0.001f, -10.0f, 10.0f, "%.4f");
        changed |= ImGui::DragFloat("Absorption Density1 Linear", &_cpuData.AbsorptionDensity1LinearTerm, 0.001f, -1.0f, 1.0f, "%.4f");
        changed |= ImGui::DragFloat3("Absorption Extinction", &_cpuData.AbsorptionExtinction.x, 0.0001f, 0.0f, 0.1f, "%.6f");

        ImGui::TextUnformatted("Ground");
        ImGui::Separator();
        changed |= ImGui::ColorEdit3("Ground Albedo", &_cpuData.GroundAlbedo.x);

        ImGui::TextUnformatted("Sun");
        ImGui::Separator();
        changed |= ImGui::DragFloat2("Sun Pitch/Yaw", &_cpuData.sun_dir.x, 0.01f, -3.1415926f, 3.1415926f, "%.3f");
        changed |= ImGui::DragFloat3("Solar Irradiance", &_cpuData.solar_irradiance.x, 0.01f, 0.0f, 20.0f, "%.3f");
        changed |= ImGui::DragFloat("Sun Angular Radius", &_cpuData.sun_angular_radius, 0.0001f, 0.0f, 0.1f, "%.6f");
        changed |= ImGui::SliderFloat("Mu S Min", &_cpuData.mu_s_min, -1.0f, 1.0f, "%.3f");
    }
    ImGui::End();

    if (changed) flushToGPU();
}

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

void VolumeSkyPass::onGUI()
{
    _skyAtmosControler.onGUI();
}

} // namespace Play
