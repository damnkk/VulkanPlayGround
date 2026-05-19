#include <rttr/registration>

#include "controlComponent/controlComponent.h"
#include "core/PlayCamera.h"
#include "core/RefCounted.h"
#include "core/runtime/RenderSession.h"
#include "core/runtime/RuntimeConfig.h"
#include "core/runtime/SdlWindow.h"
#include "core/runtime/VulkanRuntime.h"
#include "renderer/DeferRendering.h"
#include "renderer/GaussianPass/GaussianDrawMeshPass.h"
#include "renderer/GaussianPass/GaussianSortPass.h"
#include "renderer/GaussianRenderer.h"
#include "renderer/GBufferConfig.h"
#include "renderer/Renderer.h"
#include "renderer/renderPasses/GBufferPass.h"
#include "renderer/renderPasses/LightPass.h"
#include "renderer/renderPasses/PostProcessPass.h"
#include "renderer/renderPasses/PresentPass.h"
#include "renderer/renderPasses/RenderPass.h"
#include "renderer/renderPasses/VolumeSkyPass.h"
#include "resourceManagement/Material.h"
#include "resourceManagement/PlayProgram.h"
#include "resourceManagement/PlayScene.h"
#include "resourceManagement/Resource.h"
#include "resourceManagement/ShaderManager.hpp"

RTTR_REGISTRATION
{
    rttr::registration::class_<Play::runtime::RuntimeConfig>("Play::runtime::RuntimeConfig");
    rttr::registration::class_<Play::runtime::SdlInputState>("Play::runtime::SdlInputState");
    rttr::registration::class_<Play::runtime::VulkanRuntime>("Play::runtime::VulkanRuntime");
    rttr::registration::class_<Play::RenderSession>("Play::RenderSession");

    rttr::registration::class_<Play::PlayCamera>("Play::PlayCamera");
    rttr::registration::class_<Play::RefCounted>("Play::RefCounted");

    rttr::registration::class_<Play::Renderer>("Play::Renderer");
    rttr::registration::class_<Play::DeferRenderer>("Play::DeferRenderer");
    rttr::registration::class_<Play::GaussianRenderer>("Play::GaussianRenderer");

    rttr::registration::class_<Play::BasePass>("Play::BasePass");
    rttr::registration::class_<Play::GBufferPass>("Play::GBufferPass");
    rttr::registration::class_<Play::LightPass>("Play::LightPass");
    rttr::registration::class_<Play::PostProcessPass>("Play::PostProcessPass");
    rttr::registration::class_<Play::PresentPass>("Play::PresentPass");
    rttr::registration::class_<Play::VolumeSkyPass>("Play::VolumeSkyPass");
    rttr::registration::class_<Play::GaussianSortPass>("Play::GaussianSortPass");
    rttr::registration::class_<Play::GaussianDrawMeshPass>("Play::GaussianDrawMeshPass");

    rttr::registration::class_<AtmosphereParameters>("AtmosphereParameters");
    rttr::registration::class_<Play::ControlComponent<AtmosphereParameters>>("Play::ControlComponent<AtmosphereParameters>");
    rttr::registration::class_<Play::ControlComponent<shaderio::TonemapperData>>("Play::ControlComponent<shaderio::TonemapperData>");
    rttr::registration::class_<Play::ToneMappingControlComponent>("Play::ToneMappingControlComponent");

    rttr::registration::class_<Play::GBufferRTParam>("Play::GBufferRTParam");
    rttr::registration::class_<Play::GBufferConfig>("Play::GBufferConfig");

    rttr::registration::class_<Play::Material>("Play::Material");
    rttr::registration::class_<Play::FixedMaterial>("Play::FixedMaterial");
    rttr::registration::class_<Play::CustomMaterial>("Play::CustomMaterial");

    rttr::registration::class_<Play::Texture>("Play::Texture");
    rttr::registration::class_<Play::Texture::TexMetaData>("Play::Texture::TexMetaData");
    rttr::registration::class_<Play::Buffer>("Play::Buffer");
    rttr::registration::class_<Play::Buffer::BufferMetaData>("Play::Buffer::BufferMetaData");

    rttr::registration::class_<Play::RenderScene>("Play::RenderScene");
    rttr::registration::class_<Play::RTScene>("Play::RTScene");
    rttr::registration::class_<Play::GaussianScene>("Play::GaussianScene");

    rttr::registration::class_<Play::ShaderModule>("Play::ShaderModule");
    rttr::registration::class_<Play::ShaderManager>("Play::ShaderManager");

    rttr::registration::class_<Play::PlayProgram>("Play::PlayProgram");
    rttr::registration::class_<Play::RenderProgram>("Play::RenderProgram");
    rttr::registration::class_<Play::ComputeProgram>("Play::ComputeProgram");
    rttr::registration::class_<Play::RTProgram>("Play::RTProgram");
    rttr::registration::class_<Play::MeshRenderProgram>("Play::MeshRenderProgram");
}
