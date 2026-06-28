#include "VolumeRenderPass.h"

#include "VolumeRenderer.h"
#include "RDG/RDG.h"
#include "ShaderManager.hpp"
#include "core/runtime/VulkanRuntime.h"
#include "PlayAllocator.h"
#include "utils.hpp"
#include "editor/EditorRegistry.h"
#include "tinygltf/json.hpp"
#include <nvutils/file_operations.hpp>
#include <nvutils/logger.hpp>

namespace
{
constexpr uint32_t kLookupTextureSize = 512;
constexpr uint32_t kVolumeGroupSize   = 8;

template <uint32_t N>
struct PiecewiseFunction
{
    float    rangeMin = -1024.0f;
    float    rangeMax = 3071.0f;
    uint32_t Count    = 0;
    float    Position[N]{};
    float    Value[N]{};
};

template <uint32_t N = 64>
class PiecewiseLinearFunction : public PiecewiseFunction<N>
{
public:
    void addNode(float position, float value)
    {
        this->Position[this->Count] = position;
        this->Value[this->Count]    = value;
        ++this->Count;
    }

    float evaluate(float positionNormalized) const
    {
        const float position = positionNormalized * (this->rangeMax - this->rangeMin) + this->rangeMin;
        if (this->Count <= 0)
        {
            return 0.0f;
        }
        if (position < this->rangeMin)
        {
            return this->Value[0];
        }
        if (position > this->rangeMax)
        {
            return this->Value[this->Count - 1];
        }
        for (uint32_t i = 1; i < this->Count; ++i)
        {
            const float p1 = this->Position[i - 1];
            const float p2 = this->Position[i];
            if (position >= p1 && position < p2)
            {
                const float t = (position - p1) / (p2 - p1);
                return this->Value[i - 1] + t * (this->Value[i] - this->Value[i - 1]);
            }
        }
        return 0.0f;
    }
};

class ScalarTransferFunction1D
{
public:
    void addNode(float position, float value)
    {
        _function.addNode(position, value);
    }

    std::vector<uint8_t> getLookUpData(uint32_t sampleNum) const
    {
        std::vector<uint8_t> data(sampleNum);
        for (uint32_t i = 0; i < sampleNum; ++i)
        {
            data[i] = static_cast<uint8_t>(255.0f * _function.evaluate(float(i) / static_cast<float>(sampleNum - 1)) + 0.5f);
        }
        return data;
    }

private:
    PiecewiseLinearFunction<> _function;
};

class ColorTransferFunction1D
{
public:
    void addNode(float position, const glm::vec3& value)
    {
        _function0.addNode(position, value.x);
        _function1.addNode(position, value.y);
        _function2.addNode(position, value.z);
    }

    std::vector<glm::u8vec4> getLookUpData(uint32_t sampleNum) const
    {
        std::vector<glm::u8vec4> data(sampleNum);
        for (uint32_t i = 0; i < sampleNum; ++i)
        {
            const float intensity = float(i) / static_cast<float>(sampleNum - 1);
            const auto  x         = static_cast<uint8_t>(255.0f * _function0.evaluate(intensity) + 0.5f);
            const auto  y         = static_cast<uint8_t>(255.0f * _function1.evaluate(intensity) + 0.5f);
            const auto  z         = static_cast<uint8_t>(255.0f * _function2.evaluate(intensity) + 0.5f);
            data[i]               = glm::u8vec4(x, y, z, 0);
        }
        return data;
    }

private:
    PiecewiseLinearFunction<> _function0;
    PiecewiseLinearFunction<> _function1;
    PiecewiseLinearFunction<> _function2;
};

uint32_t divRoundUp(uint32_t value, uint32_t divisor)
{
    return (value + divisor - 1) / divisor;
}

bool sameMat4(const glm::mat4& lhs, const glm::mat4& rhs)
{
    for (uint32_t column = 0; column < 4; ++column)
    {
        for (uint32_t row = 0; row < 4; ++row)
        {
            if (lhs[column][row] != rhs[column][row])
            {
                return false;
            }
        }
    }
    return true;
}

bool sameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool sameParameters(const Play::VolumeRenderParameters& lhs, const Play::VolumeRenderParameters& rhs)
{
    return lhs.Density == rhs.Density && lhs.Exposure == rhs.Exposure && lhs.StepCount == rhs.StepCount && sameVec3(lhs.BBoxMin, rhs.BBoxMin)
           && sameVec3(lhs.BBoxMax, rhs.BBoxMax);
}

VkImageCreateInfo makeImageCreateInfo(VkImageType type, VkFormat format, VkExtent3D extent, VkImageUsageFlags usage)
{
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType     = type;
    imageInfo.format        = format;
    imageInfo.extent        = extent;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage         = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return imageInfo;
}

VkImageViewCreateInfo makeImageViewCreateInfo(VkImageViewType type, VkFormat format)
{
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.viewType         = type;
    viewInfo.format           = format;
    viewInfo.components       = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    viewInfo.subresourceRange = {Play::inferImageAspectFlags(format, true), 0, 1, 0, 1};
    return viewInfo;
}

uint16_t readUint16(const std::string& data, size_t index)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(data.data() + index * sizeof(uint16_t));
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

glm::vec3 vec3FromJson(const nlohmann::json& value)
{
    return glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
}
} // namespace

namespace Play
{

VolumeRenderPass::VolumeRenderPass(VolumeRenderer* renderer) : _ownedRenderer(renderer) {}

VolumeRenderPass::~VolumeRenderPass() = default;

void VolumeRenderPass::init()
{
    loadVolumeTexture();
    createTransferFunctionTextures();
    createEnvTexture();
    createUniformBuffer();

    const uint32_t gradientId = ShaderManager::Instance().loadShaderFromFile("volumeGradient", "volumeRender/gradiant.comp", ShaderStage::eCompute,
                                                                              ShaderType::eGLSL, "main");
    const uint32_t genRayId   = ShaderManager::Instance().loadShaderFromFile("volumeRenderGenRay", "volumeRender/volumeGenRay.comp", ShaderStage::eCompute,
                                                                            ShaderType::eGLSL, "main");
    const uint32_t radianceId = ShaderManager::Instance().loadShaderFromFile("volumeRadiance", "volumeRender/volumeRadiance.comp", ShaderStage::eCompute,
                                                                              ShaderType::eGLSL, "main");
    const uint32_t accumulateId = ShaderManager::Instance().loadShaderFromFile("volumeAccumulate", "volumeRender/volumeAccumulate.comp",
                                                                                ShaderStage::eCompute, ShaderType::eGLSL, "main");
    const uint32_t postProcessId = ShaderManager::Instance().loadShaderFromFile("volumePostProcess", "volumeRender/volumePostProcess.comp",
                                                                                 ShaderStage::eCompute, ShaderType::eGLSL, "main");

    _gradientPipeline.setShader(gradientId);
    _generateRaysPipeline.setShader(genRayId);
    _radiancePipeline.setShader(radianceId);
    _accumulatePipeline.setShader(accumulateId);
    _postProcessPipeline.setShader(postProcessId);

    _lastParameters = _parameters;
    vkDriver->getEditorRegistry().registerWritable<VolumeRenderParameters>("Volume", _parameters, editor::EditorRenderMode::Volume);
}

void VolumeRenderPass::build(RDG::RDGBuilder* rdgBuilder)
{
    auto volumeTextureRef      = rdgBuilder->createTexture("VolumeTexture").Import(_textures[eVolumeTexture].get()).finish();
    auto gradientTextureRef    = rdgBuilder->createTexture("VolumeGradientTexture").Import(_textures[eGradientTexture].get()).finish();
    auto diffuseLookupRef      = rdgBuilder->createTexture("VolumeDiffuseLookupTexture").Import(_textures[eDiffuseLookUpTexture].get()).finish();
    auto specularLookupRef     = rdgBuilder->createTexture("VolumeSpecularLookupTexture").Import(_textures[eSpecularLookUpTexture].get()).finish();
    auto roughnessLookupRef    = rdgBuilder->createTexture("VolumeRoughnessLookupTexture").Import(_textures[eRoughnessLookUpTexture].get()).finish();
    auto opacityLookupRef      = rdgBuilder->createTexture("VolumeOpacityLookupTexture").Import(_textures[eOpacityLookUpTexture].get()).finish();
    auto envTextureRef         = rdgBuilder->createTexture("VolumeEnvTexture").Import(_textures[eEnvTexture].get()).finish();
    auto uniformBufferRef      = rdgBuilder->createBuffer("VolumeUniformBuffer").Import(_uniformBuffer.get()).finish();
    auto diffuseRTRef          = rdgBuilder->createTexture("VolumeDiffuseRT")
                            .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                            .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                            .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                            .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                            .MipmapLevel(1)
                            .finish();
    auto specularRTRef         = rdgBuilder->createTexture("VolumeSpecularRT")
                             .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                             .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                             .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                             .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                             .MipmapLevel(1)
                             .finish();
    auto normalRTRef           = rdgBuilder->createTexture("VolumeNormalRT")
                           .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                           .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                           .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                           .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                           .MipmapLevel(1)
                           .finish();
    auto depthRTRef            = rdgBuilder->createTexture("VolumeDepthRT")
                          .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                          .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                          .Format(VK_FORMAT_R32_SFLOAT)
                          .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                          .MipmapLevel(1)
                          .finish();
    auto radianceRTRef         = rdgBuilder->createTexture("VolumeRadianceRT")
                             .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                             .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                             .Format(VK_FORMAT_R16G16B16A16_SFLOAT)
                             .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                             .MipmapLevel(1)
                             .finish();
    auto accumulateRTRef       = rdgBuilder->createTexture("VolumeAccumulateRT")
                               .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                               .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                               .Format(VK_FORMAT_R32G32B32A32_SFLOAT)
                               .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                               .MipmapLevel(1)
                               .finish();
    auto outputTextureRef      = rdgBuilder->createTexture("outputTexture")
                                .Extent({vkDriver->getViewportSize().width, vkDriver->getViewportSize().height, 1})
                                .AspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
                                .Format(VK_FORMAT_R8G8B8A8_UNORM)
                                .UsageFlags(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
                                .MipmapLevel(1)
                                .finish();

    [[maybe_unused]] auto gradientPass = rdgBuilder->createComputePass("VolumeGradientPass")
                            .sampledRead(0, volumeTextureRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
                            .storageWrite(1, gradientTextureRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
                            .execute(
                                [this](RDG::PassNode* passNode, RDG::RenderContext& context)
                                {
                                    if (_gradientGenerated)
                                    {
                                        return;
                                    }
                                    context.bindPipeline(_gradientPipeline);
                                    vkCmdDispatch(context._currCmdBuffer, divRoundUp(_volumeExtent.width, kVolumeGroupSize),
                                                  divRoundUp(_volumeExtent.height, kVolumeGroupSize), divRoundUp(_volumeExtent.depth, kVolumeGroupSize));
                                    _gradientGenerated = true;
                                })
                            .finish();

    [[maybe_unused]] auto genRayPass = rdgBuilder->createComputePass("VolumeGenRayPass")
        .sampledRead(0, volumeTextureRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(1, gradientTextureRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(2, diffuseLookupRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(3, specularLookupRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(4, roughnessLookupRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(5, opacityLookupRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .read(6, uniformBufferRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageWrite(7, diffuseRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageWrite(8, specularRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageWrite(9, normalRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageWrite(10, depthRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .execute(
            [this](RDG::PassNode* passNode, RDG::RenderContext& context)
            {
                context.bindPipeline(_generateRaysPipeline);
                vkCmdDispatch(context._currCmdBuffer, divRoundUp(vkDriver->getViewportSize().width, kVolumeGroupSize),
                              divRoundUp(vkDriver->getViewportSize().height, kVolumeGroupSize), 1);
            })
        .finish();

    [[maybe_unused]] auto radiancePass = rdgBuilder->createComputePass("VolumeRadiancePass")
        .sampledRead(0, volumeTextureRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(1, opacityLookupRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(2, diffuseRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(3, specularRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(4, normalRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(5, depthRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .sampledRead(6, envTextureRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .read(7, uniformBufferRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageWrite(8, radianceRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .execute(
            [this](RDG::PassNode* passNode, RDG::RenderContext& context)
            {
                context.bindPipeline(_radiancePipeline);
                vkCmdDispatch(context._currCmdBuffer, divRoundUp(vkDriver->getViewportSize().width, kVolumeGroupSize),
                              divRoundUp(vkDriver->getViewportSize().height, kVolumeGroupSize), 1);
            })
        .finish();

    [[maybe_unused]] auto accumulatePass = rdgBuilder->createComputePass("VolumeAccumulatePass")
        .sampledRead(0, radianceRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .read(1, uniformBufferRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageReadWrite(2, accumulateRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .execute(
            [this](RDG::PassNode* passNode, RDG::RenderContext& context)
            {
                context.bindPipeline(_accumulatePipeline);
                vkCmdDispatch(context._currCmdBuffer, divRoundUp(vkDriver->getViewportSize().width, kVolumeGroupSize),
                              divRoundUp(vkDriver->getViewportSize().height, kVolumeGroupSize), 1);
            })
        .finish();

    [[maybe_unused]] auto postProcessPass = rdgBuilder->createComputePass("VolumePostProcessPass")
        .sampledRead(0, accumulateRTRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .read(1, uniformBufferRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .storageWrite(2, outputTextureRef, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT)
        .execute(
            [this](RDG::PassNode* passNode, RDG::RenderContext& context)
            {
                context.bindPipeline(_postProcessPipeline);
                vkCmdDispatch(context._currCmdBuffer, divRoundUp(vkDriver->getViewportSize().width, kVolumeGroupSize),
                              divRoundUp(vkDriver->getViewportSize().height, kVolumeGroupSize), 1);
            })
        .finish();
}

void VolumeRenderPass::updateUniform()
{
    if (!_uniformBuffer || !_ownedRenderer)
    {
        return;
    }

    const CameraData& cameraData = _ownedRenderer->getCurrentCameraData();
    if (!sameMat4(cameraData.viewMatrix, _lastViewMatrix) || !sameMat4(cameraData.projMatrix, _lastProjectMatrix) || parametersChanged())
    {
        resetFrameCount();
        _lastViewMatrix    = cameraData.viewMatrix;
        _lastProjectMatrix = cameraData.projMatrix;
        _lastParameters    = _parameters;
    }

    VolumeUniformData data{};
    data.ProjectMatrix    = cameraData.projMatrix;
    data.ViewMatrix       = cameraData.viewMatrix;
    data.WorldMatrix      = glm::mat4(1.0f);
    data.NormalMatrix     = glm::mat4(1.0f);
    data.InvProjectMatrix = cameraData.invProjMatrix;
    data.InvViewMatrix    = cameraData.invViewMatrix;
    data.InvWorldMatrix   = glm::inverse(data.WorldMatrix);
    data.InvNormalMatrix  = glm::inverse(data.NormalMatrix);
    data.frameCount       = _frameCount++;
    data.BBoxMin          = _parameters.BBoxMin;
    data.BBoxMax          = _parameters.BBoxMax;
    const uint32_t stepCount = _parameters.StepCount > 0 ? _parameters.StepCount : 1;
    data.StepSize           = glm::distance(data.BBoxMin, data.BBoxMax) / static_cast<float>(stepCount);
    data.FrameOffset      = glm::vec2(1.0f);
    data.RTDimensions     = glm::vec2(vkDriver->getViewportSize().width, vkDriver->getViewportSize().height);
    data.Density          = _parameters.Density;
    data.Exposure         = _parameters.Exposure;
    data.CameraPos        = cameraData.cameraPosition;

    memcpy(_uniformBuffer->mapping, &data, sizeof(VolumeUniformData));
    PlayResourceManager::Instance().flushBuffer(*_uniformBuffer, 0, VK_WHOLE_SIZE);
}

void VolumeRenderPass::resetFrameCount()
{
    _frameCount = 0;
}

void VolumeRenderPass::loadVolumeTexture()
{
    const std::filesystem::path volumePath = getBaseFilePath() / "content/volumeData/Textures/manix.dat";
    const std::string           fileContents = nvutils::loadFile(volumePath);
    if (fileContents.empty())
    {
        LOGE("Failed to open volume data file: %s", nvutils::utf8FromPath(volumePath).c_str());
        return;
    }

    if (fileContents.size() < sizeof(uint16_t) * 3)
    {
        LOGE("Volume data file is missing dimensions: %s", nvutils::utf8FromPath(volumePath).c_str());
        return;
    }

    uint16_t width  = readUint16(fileContents, 0);
    uint16_t height = readUint16(fileContents, 1);
    uint16_t depth  = readUint16(fileContents, 2);
    _volumeExtent   = {width, height, depth};

    const size_t voxelCount = size_t(_volumeExtent.width) * size_t(_volumeExtent.height) * size_t(_volumeExtent.depth);
    if (fileContents.size() < sizeof(uint16_t) * (voxelCount + 3))
    {
        LOGE("Volume data file is truncated: %s", nvutils::utf8FromPath(volumePath).c_str());
        return;
    }
    std::vector<uint16_t> intensityData(voxelCount);
    for (size_t index = 0; index < voxelCount; ++index)
    {
        intensityData[index] = readUint16(fileContents, index + 3);
    }

    const uint16_t tmin = 0 << 12;
    const uint16_t tmax = 1 << 12;
    for (uint16_t& intensity : intensityData)
    {
        intensity = static_cast<uint16_t>(65535.0f * ((intensity - tmin) / static_cast<float>(tmax - tmin)) + 0.5f);
    }

    _textures[eVolumeTexture] = RefPtr<Texture>(new Texture());
    VkImageCreateInfo volumeImageInfo = makeImageCreateInfo(VK_IMAGE_TYPE_3D, VK_FORMAT_R16_UNORM, _volumeExtent,
                                                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    VkImageViewCreateInfo volumeViewInfo = makeImageViewCreateInfo(VK_IMAGE_VIEW_TYPE_3D, VK_FORMAT_R16_UNORM);
    uploadTexture(*_textures[eVolumeTexture], intensityData.data(), intensityData.size() * sizeof(uint16_t), volumeImageInfo, volumeViewInfo,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _textures[eVolumeTexture]->DebugName() = "VolumeTexture";

    _textures[eGradientTexture] = RefPtr<Texture>(new Texture(_volumeExtent.width, _volumeExtent.height, _volumeExtent.depth,
                                                              VK_FORMAT_R16G16B16A16_SFLOAT,
                                                              VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                              VK_IMAGE_LAYOUT_GENERAL));
    _textures[eGradientTexture]->DebugName() = "VolumeGradientTexture";
    acquireLinearSampler(*_textures[eGradientTexture]);
}

void VolumeRenderPass::createTransferFunctionTextures()
{
    const std::filesystem::path transferFunctionPath = getBaseFilePath() / "content/volumeData/TransferFunctions/ManixTransferFunction.json";
    const std::string           fileContents         = nvutils::loadFile(transferFunctionPath);
    if (fileContents.empty())
    {
        LOGE("Failed to open transfer function file: %s", nvutils::utf8FromPath(transferFunctionPath).c_str());
        return;
    }

    const nlohmann::json root = nlohmann::json::parse(fileContents);
    ColorTransferFunction1D diffuseTransferFunc;
    ColorTransferFunction1D specularTransferFunc;
    ScalarTransferFunction1D roughnessTransferFunc;
    ScalarTransferFunction1D opacityTransferFunc;

    for (const auto& node : root["NodesColor"])
    {
        const float intensity = node["Intensity"].get<float>();
        diffuseTransferFunc.addNode(intensity, vec3FromJson(node["Diffuse"]));
        specularTransferFunc.addNode(intensity, vec3FromJson(node["Specular"]));
        roughnessTransferFunc.addNode(intensity, node["Roughness"].get<float>());
    }
    for (const auto& node : root["NodesOpacity"])
    {
        opacityTransferFunc.addNode(node["Intensity"].get<float>(), node["Opacity"].get<float>());
    }

    const auto diffuseData   = diffuseTransferFunc.getLookUpData(kLookupTextureSize);
    const auto specularData  = specularTransferFunc.getLookUpData(kLookupTextureSize);
    const auto roughnessData = roughnessTransferFunc.getLookUpData(kLookupTextureSize);
    const auto opacityData   = opacityTransferFunc.getLookUpData(kLookupTextureSize);

    VkExtent3D lookupExtent = {kLookupTextureSize, 1, 1};
    VkImageCreateInfo rgbaLookupImageInfo = makeImageCreateInfo(VK_IMAGE_TYPE_1D, VK_FORMAT_R8G8B8A8_UNORM, lookupExtent, VK_IMAGE_USAGE_SAMPLED_BIT);
    VkImageViewCreateInfo rgbaLookupViewInfo = makeImageViewCreateInfo(VK_IMAGE_VIEW_TYPE_1D, VK_FORMAT_R8G8B8A8_UNORM);
    VkImageCreateInfo scalarLookupImageInfo = makeImageCreateInfo(VK_IMAGE_TYPE_1D, VK_FORMAT_R8_UNORM, lookupExtent, VK_IMAGE_USAGE_SAMPLED_BIT);
    VkImageViewCreateInfo scalarLookupViewInfo = makeImageViewCreateInfo(VK_IMAGE_VIEW_TYPE_1D, VK_FORMAT_R8_UNORM);

    _textures[eDiffuseLookUpTexture] = RefPtr<Texture>(new Texture());
    uploadTexture(*_textures[eDiffuseLookUpTexture], diffuseData.data(), diffuseData.size() * sizeof(glm::u8vec4), rgbaLookupImageInfo,
                  rgbaLookupViewInfo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _textures[eDiffuseLookUpTexture]->DebugName() = "VolumeDiffuseLookupTexture";

    _textures[eSpecularLookUpTexture] = RefPtr<Texture>(new Texture());
    uploadTexture(*_textures[eSpecularLookUpTexture], specularData.data(), specularData.size() * sizeof(glm::u8vec4), rgbaLookupImageInfo,
                  rgbaLookupViewInfo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _textures[eSpecularLookUpTexture]->DebugName() = "VolumeSpecularLookupTexture";

    _textures[eRoughnessLookUpTexture] = RefPtr<Texture>(new Texture());
    uploadTexture(*_textures[eRoughnessLookUpTexture], roughnessData.data(), roughnessData.size() * sizeof(uint8_t), scalarLookupImageInfo,
                  scalarLookupViewInfo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _textures[eRoughnessLookUpTexture]->DebugName() = "VolumeRoughnessLookupTexture";

    _textures[eOpacityLookUpTexture] = RefPtr<Texture>(new Texture());
    uploadTexture(*_textures[eOpacityLookUpTexture], opacityData.data(), opacityData.size() * sizeof(uint8_t), scalarLookupImageInfo,
                  scalarLookupViewInfo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _textures[eOpacityLookUpTexture]->DebugName() = "VolumeOpacityLookupTexture";
}

void VolumeRenderPass::createEnvTexture()
{
    const std::filesystem::path envPath = getBaseFilePath() / "resource/skybox/test.hdr";
    _textures[eEnvTexture]             = RefPtr<Texture>(new Texture(envPath, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, true));
    _textures[eEnvTexture]->DebugName() = "VolumeEnvTexture";
}

void VolumeRenderPass::createUniformBuffer()
{
    _uniformBuffer = RefPtr<Buffer>(new Buffer("VolumeUniformBuffer", VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT, sizeof(VolumeUniformData),
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
}

void VolumeRenderPass::uploadTexture(Texture& texture, const void* data, VkDeviceSize dataSize, VkImageCreateInfo imageInfo, VkImageViewCreateInfo viewInfo,
                                     VkImageLayout finalLayout)
{
    PlayResourceManager::Instance().createImage(texture, imageInfo, viewInfo);
    auto cmd = PlayResourceManager::Instance().getTempCommandBuffer();
    PlayResourceManager::Instance().appendImage(texture, dataSize, data, finalLayout);
    PlayResourceManager::Instance().cmdUploadAppended(cmd);
    PlayResourceManager::Instance().submitAndWaitTempCmdBuffer(cmd);

    texture.Layout()      = finalLayout;
    texture.Format()      = imageInfo.format;
    texture.Type()        = imageInfo.imageType;
    texture.Extent()      = imageInfo.extent;
    texture.SampleCount() = imageInfo.samples;
    texture.UsageFlags()  = imageInfo.usage;
    texture.AspectFlags() = inferImageAspectFlags(imageInfo.format, false);
    texture.MipLevel()    = imageInfo.mipLevels;
    texture.LayerCount()  = imageInfo.arrayLayers;
    acquireLinearSampler(texture);
}

void VolumeRenderPass::acquireLinearSampler(Texture& texture)
{
    if (texture.descriptor.sampler != VK_NULL_HANDLE)
    {
        return;
    }
    VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerCreateInfo.magFilter     = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter     = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.maxAnisotropy = 1.0f;
    PlayResourceManager::Instance().acquireSampler(texture.descriptor.sampler, samplerCreateInfo);
}

bool VolumeRenderPass::parametersChanged() const
{
    return !sameParameters(_parameters, _lastParameters);
}

} // namespace Play
