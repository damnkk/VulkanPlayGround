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
#include "renderer/VolumeRenderer.h"
#include "renderer/renderPasses/GBufferPass.h"
#include "renderer/renderPasses/LightPass.h"
#include "renderer/renderPasses/PostProcessPass.h"
#include "renderer/renderPasses/PresentPass.h"
#include "renderer/renderPasses/RenderPass.h"
#include "renderer/renderPasses/VolumeSkyPass.h"
#include "renderer/renderPasses/VolumeRenderPass.h"
#include "resourceManagement/Material.h"
#include "resourceManagement/PlayScene.h"
#include "resourceManagement/Resource.h"
#include "resourceManagement/SceneManager.h"
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
    rttr::registration::class_<Play::VolumeRenderer>("Play::VolumeRenderer");

    rttr::registration::class_<Play::BasePass>("Play::BasePass");
    rttr::registration::class_<Play::GBufferPass>("Play::GBufferPass");
    rttr::registration::class_<Play::LightPass>("Play::LightPass");
    rttr::registration::class_<Play::PostProcessPass>("Play::PostProcessPass");
    rttr::registration::class_<Play::PresentPass>("Play::PresentPass");
    rttr::registration::class_<Play::VolumeSkyPass>("Play::VolumeSkyPass");
    rttr::registration::class_<Play::VolumeRenderPass>("Play::VolumeRenderPass");
    rttr::registration::class_<Play::GaussianSortPass>("Play::GaussianSortPass");
    rttr::registration::class_<Play::GaussianDrawMeshPass>("Play::GaussianDrawMeshPass");

    rttr::registration::class_<AtmosphereParameters>("AtmosphereParameters")
        .property("BottomRadius", &AtmosphereParameters::BottomRadius)(rttr::metadata("ui.label", "Bottom Radius"))
        .property("TopRadius", &AtmosphereParameters::TopRadius)(rttr::metadata("ui.label", "Top Radius"))
        .property("RayleighDensityExpScale", &AtmosphereParameters::RayleighDensityExpScale)
        .property("RayleighScattering", &AtmosphereParameters::RayleighScattering)
        .property("MieDensityExpScale", &AtmosphereParameters::MieDensityExpScale)
        .property("MieScattering", &AtmosphereParameters::MieScattering)
        .property("MieExtinction", &AtmosphereParameters::MieExtinction)
        .property("MieAbsorption", &AtmosphereParameters::MieAbsorption)
        .property("MiePhaseG", &AtmosphereParameters::MiePhaseG)(
            rttr::metadata("ui.label", "Mie Phase G"),
            rttr::metadata("ui.widget", "slider"),
            rttr::metadata("ui.min", -1.0f),
            rttr::metadata("ui.max", 1.0f),
            rttr::metadata("ui.step", 0.01f))
        .property("AbsorptionDensity0LayerWidth", &AtmosphereParameters::AbsorptionDensity0LayerWidth)
        .property("AbsorptionDensity0ConstantTerm", &AtmosphereParameters::AbsorptionDensity0ConstantTerm)
        .property("AbsorptionDensity0LinearTerm", &AtmosphereParameters::AbsorptionDensity0LinearTerm)
        .property("AbsorptionDensity1ConstantTerm", &AtmosphereParameters::AbsorptionDensity1ConstantTerm)
        .property("AbsorptionDensity1LinearTerm", &AtmosphereParameters::AbsorptionDensity1LinearTerm)
        .property("AbsorptionExtinction", &AtmosphereParameters::AbsorptionExtinction)
        .property("GroundAlbedo", &AtmosphereParameters::GroundAlbedo)
        .property("sun_dir", &AtmosphereParameters::sun_dir)(rttr::metadata("ui.label", "Sun Direction"))
        .property("solar_irradiance", &AtmosphereParameters::solar_irradiance)
        .property("sun_angular_radius", &AtmosphereParameters::sun_angular_radius)
        .property("mu_s_min", &AtmosphereParameters::mu_s_min);

    rttr::registration::class_<Play::VolumeRenderParameters>("Play::VolumeRenderParameters")
        .property("Density", &Play::VolumeRenderParameters::Density)(
            rttr::metadata("ui.widget", "slider"), rttr::metadata("ui.min", 0.0f), rttr::metadata("ui.max", 500.0f),
            rttr::metadata("ui.step", 1.0f))
        .property("Exposure", &Play::VolumeRenderParameters::Exposure)(
            rttr::metadata("ui.widget", "slider"), rttr::metadata("ui.min", 0.0f), rttr::metadata("ui.max", 8.0f),
            rttr::metadata("ui.step", 0.01f))
        .property("StepCount", &Play::VolumeRenderParameters::StepCount)
        .property("BBoxMin", &Play::VolumeRenderParameters::BBoxMin)
        .property("BBoxMax", &Play::VolumeRenderParameters::BBoxMax);

    rttr::registration::class_<shaderio::TonemapperData>("shaderio::TonemapperData")
        .property("isActive", &shaderio::TonemapperData::isActive)
        .property("method", &shaderio::TonemapperData::method)
        .property("exposure", &shaderio::TonemapperData::exposure)(
            rttr::metadata("ui.widget", "slider"),
            rttr::metadata("ui.min", 0.0f),
            rttr::metadata("ui.max", 8.0f),
            rttr::metadata("ui.step", 0.01f))
        .property("temperature", &shaderio::TonemapperData::temperature)
        .property("tint", &shaderio::TonemapperData::tint)
        .property("contrast", &shaderio::TonemapperData::contrast)
        .property("brightness", &shaderio::TonemapperData::brightness)
        .property("saturation", &shaderio::TonemapperData::saturation)
        .property("vignette", &shaderio::TonemapperData::vignette)
        .property("vibrance", &shaderio::TonemapperData::vibrance)
        .property("shadowBias", &shaderio::TonemapperData::shadowBias)
        .property("midtoneBias", &shaderio::TonemapperData::midtoneBias)
        .property("highlightBias", &shaderio::TonemapperData::highlightBias)
        .property("coolColor", &shaderio::TonemapperData::coolColor)
        .property("warmColor", &shaderio::TonemapperData::warmColor)
        .property("splitBalance", &shaderio::TonemapperData::splitBalance)
        .property("autoExposure", &shaderio::TonemapperData::autoExposure)
        .property("autoExposureSpeed", &shaderio::TonemapperData::autoExposureSpeed)
        .property("evMinValue", &shaderio::TonemapperData::evMinValue)
        .property("evMaxValue", &shaderio::TonemapperData::evMaxValue)
        .property("enableCenterMetering", &shaderio::TonemapperData::enableCenterMetering)
        .property("centerMeteringSize", &shaderio::TonemapperData::centerMeteringSize)
        .property("averageMode", &shaderio::TonemapperData::averageMode)
        .property("dither", &shaderio::TonemapperData::dither);

    rttr::registration::class_<Play::ControlComponent<AtmosphereParameters>>("Play::ControlComponent<AtmosphereParameters>");
    rttr::registration::class_<Play::ControlComponent<shaderio::TonemapperData>>("Play::ControlComponent<shaderio::TonemapperData>");
    rttr::registration::class_<Play::ToneMappingControlComponent>("Play::ToneMappingControlComponent");

    rttr::registration::class_<Play::GBufferRTParam>("Play::GBufferRTParam");
    rttr::registration::class_<Play::GBufferConfig>("Play::GBufferConfig");

    rttr::registration::class_<Play::Material>("Play::Material");
    rttr::registration::class_<Play::Texture>("Play::Texture");
    rttr::registration::class_<Play::Texture::TexMetaData>("Play::Texture::TexMetaData");
    rttr::registration::class_<Play::Buffer>("Play::Buffer");
    rttr::registration::class_<Play::Buffer::BufferMetaData>("Play::Buffer::BufferMetaData");

    rttr::registration::class_<Play::GaussianScene>("Play::GaussianScene");
    rttr::registration::class_<Play::SceneManager>("Play::SceneManager");

    rttr::registration::class_<Play::ShaderModule>("Play::ShaderModule");
    rttr::registration::class_<Play::ShaderManager>("Play::ShaderManager");
}
