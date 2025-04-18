#include "VolumeRenderer.h"
#include "json.hpp"
#include "nvvk/images_vk.hpp"
#include "utils.hpp"
#include "nvh/geometry.hpp"
#include <PlayApp.h>
#include "nvh/misc.hpp"
namespace Play
{
struct VolumeTexture : public Texture
{
    VkExtent3D _extent = {0, 0, 0};
};

VolumeRenderer::VolumeRenderer(PlayApp& app)
{
    _app = &app;
    createRenderResource();
}
void VolumeRenderer::OnPreRender() {}
void VolumeRenderer::OnPostRender() {}
void VolumeRenderer::RenderFrame() {}
void VolumeRenderer::SetScene(Scene* scene) {}
void VolumeRenderer::OnResize(int width, int height) {}
void VolumeRenderer::OnDestroy()
{
    vkDestroyImage(_app->m_device, _depthRT.image, nullptr);
    vkFreeMemory(_app->m_device, _depthRT.memory, nullptr);
    vkDestroyImageView(_app->m_device, _depthRT.view, nullptr);
}
void VolumeRenderer::loadVolumeTexture(const std::string& filename)
{
    std::unique_ptr<FILE, decltype(&fclose)> file(fopen(filename.c_str(), "rb"), fclose);
    if (!file)
    {
        throw std::runtime_error("Failed to open volume data file.");
    }
    _volumeTexture    = static_cast<VolumeTexture*>(PlayApp::AllocTexture<VolumeTexture>());
    uint16_t tempSize = 0;
    fread(&tempSize, sizeof(uint16_t), 1, file.get());
    _volumeTexture->_extent.width = tempSize;
    fread(&tempSize, sizeof(uint16_t), 1, file.get());
    _volumeTexture->_extent.height = tempSize;
    fread(&tempSize, sizeof(uint16_t), 1, file.get());
    _volumeTexture->_extent.depth = tempSize;
    std::vector<uint16_t> intensityData(size_t(_volumeTexture->_extent.width) *
                                        size_t(_volumeTexture->_extent.height) *
                                        size_t(_volumeTexture->_extent.depth));
    fread(intensityData.data(), sizeof(uint16_t), intensityData.size(), file.get());
    uint32_t mipLevels = nvh::mipMapLevels(
        std::max(_volumeTexture->_extent.width,
                 std::max(_volumeTexture->_extent.height, _volumeTexture->_extent.depth)));
    uint16_t tmin               = 0 << 12;
    uint16_t tmax               = 1 << 12;
    auto     normalizeIntensity = [&tmin, &tmax](uint16_t& intensity)
    {
        intensity = static_cast<uint16_t>(
            std::round(std::numeric_limits<uint16_t>::max() *
                       ((intensity - tmin) / static_cast<float>(tmax - tmin))));
    };

    std::for_each(intensityData.begin(), intensityData.end(), normalizeIntensity);
    VkImageCreateInfo imageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageCreateInfo.imageType     = VK_IMAGE_TYPE_3D;
    imageCreateInfo.extent.width  = _volumeTexture->_extent.width;
    imageCreateInfo.extent.height = _volumeTexture->_extent.height;
    imageCreateInfo.extent.depth  = _volumeTexture->_extent.depth;
    imageCreateInfo.format        = VK_FORMAT_R16_UNORM;
    imageCreateInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageCreateInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.arrayLayers   = 1;
    imageCreateInfo.mipLevels     = mipLevels;
    imageCreateInfo.flags         = 0;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.pNext         = nullptr;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto          cmd               = _app->createTempCmdBuffer();
    nvvk::Texture nvvkVolumeTexture = _app->_alloc.createTexture(
        cmd, intensityData.size() * sizeof(uint16_t), intensityData.data(), imageCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _app->submitTempCmdBuffer(cmd);
    int a = 0;
}
void VolumeRenderer::createRenderResource()
{
    // create volume texture
    loadVolumeTexture("content/volumeData/Textures/manix.dat");
    // create lookup texture
    nlohmann::json root;
    std::ifstream  fin("./content/volumeData/TransferFunctions/ManixTransferFunction.json");
    fin >> root;
    auto ExtractVec3FromJson = [](auto const& tree, auto const& key) -> glm::vec3
    {
        glm::vec3 v{};
        v.x = tree[key][0].template get<float>();
        v.y = tree[key][1].template get<float>();
        v.z = tree[key][2].template get<float>();
        return v;
    };
    for (auto const& e : root["NodesColor"])
    {
        const auto intensity = e["Intensity"].get<float>();
        const auto diffuse   = ExtractVec3FromJson(e, "Diffuse");
        const auto specular  = ExtractVec3FromJson(e, "Specular");
        const auto roughness = e["Roughness"].get<float>();

        _diffuseTransferFunc.addNode(intensity, diffuse);
        _specularTransferFunc.addNode(intensity, specular);
        _roughnessTransferFunc.addNode(intensity, roughness);
    }
    for (auto const& e : root["NodesOpacity"])
        _opacityTransferFunc.addNode(e["Intensity"].get<float>(), e["Opacity"].get<float>());
    auto diffuseData   = _diffuseTransferFunc.getLookUpData(512);
    auto specularData  = _specularTransferFunc.getLookUpData(512);
    auto roughnessData = _roughnessTransferFunc.getLookUpData(512);
    auto opacityData   = _opacityTransferFunc.getLookUpData(512);

    VkImageCreateInfo image1DCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image1DCreateInfo.extent    = {512, 1, 1};
    image1DCreateInfo.imageType = VK_IMAGE_TYPE_1D;
    image1DCreateInfo.format    = VK_FORMAT_R8_UNORM;
    image1DCreateInfo.usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image1DCreateInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    image1DCreateInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    image1DCreateInfo.arrayLayers = 1;
    image1DCreateInfo.mipLevels   = 1;
    image1DCreateInfo.flags       = 0;
    image1DCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkImageCreateInfo image2DCreateInfo =
        nvvk::makeImage2DCreateInfo({512, 1}, VK_FORMAT_R8G8_UNORM,
                                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false);
    _diffuseLookUpTexture                  = PlayApp::AllocTexture<Texture>();
    _specularLookUpTexture                 = PlayApp::AllocTexture<Texture>();
    _roughnessLookUpTexture                = PlayApp::AllocTexture<Texture>();
    _opacityLookUpTexture                  = PlayApp::AllocTexture<Texture>();
    auto          cmd                      = _app->createTempCmdBuffer();
    nvvk::Texture nvvkDiffuseLookUpTexture = _app->_alloc.createTexture(
        cmd, diffuseData.size() * sizeof(glm::vec4), diffuseData.data(), image2DCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _diffuseLookUpTexture->image      = nvvkDiffuseLookUpTexture.image;
    _diffuseLookUpTexture->memHandle  = nvvkDiffuseLookUpTexture.memHandle;
    _diffuseLookUpTexture->descriptor = nvvkDiffuseLookUpTexture.descriptor;
    _diffuseLookUpTexture->_format    = image2DCreateInfo.format;
    CUSTOM_NAME_VK(_app->m_debug, _diffuseLookUpTexture->image);
    nvvk::Texture nvvkSpecularLookUpTexture = _app->_alloc.createTexture(
        cmd, specularData.size() * sizeof(glm::vec4), specularData.data(), image2DCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _specularLookUpTexture->image      = nvvkSpecularLookUpTexture.image;
    _specularLookUpTexture->memHandle  = nvvkSpecularLookUpTexture.memHandle;
    _specularLookUpTexture->descriptor = nvvkSpecularLookUpTexture.descriptor;
    _specularLookUpTexture->_format    = image2DCreateInfo.format;
    CUSTOM_NAME_VK(_app->m_debug, _specularLookUpTexture->image);
    nvvk::Texture nvvkRoughnessLoopUpTexture = _app->_alloc.createTexture(
        cmd, roughnessData.size() * sizeof(uint8_t), roughnessData.data(), image1DCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _roughnessLookUpTexture->image      = nvvkRoughnessLoopUpTexture.image;
    _roughnessLookUpTexture->memHandle  = nvvkRoughnessLoopUpTexture.memHandle;
    _roughnessLookUpTexture->descriptor = nvvkRoughnessLoopUpTexture.descriptor;
    _roughnessLookUpTexture->_format    = image1DCreateInfo.format;
    CUSTOM_NAME_VK(_app->m_debug, _roughnessLookUpTexture->image);
    nvvk::Texture nvvkOpacityLookUpTexture = _app->_alloc.createTexture(
        cmd, opacityData.size() * sizeof(uint8_t), opacityData.data(), image1DCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _opacityLookUpTexture->image      = nvvkOpacityLookUpTexture.image;
    _opacityLookUpTexture->memHandle  = nvvkOpacityLookUpTexture.memHandle;
    _opacityLookUpTexture->descriptor = nvvkOpacityLookUpTexture.descriptor;
    _opacityLookUpTexture->_format    = image1DCreateInfo.format;
    CUSTOM_NAME_VK(_app->m_debug, _opacityLookUpTexture->image);
    _app->submitTempCmdBuffer(cmd);

    // create multiple render target
    _diffuseRT                = PlayApp::AllocTexture<Texture>();
    _specularRT               = PlayApp::AllocTexture<Texture>();
    _radianceRT               = PlayApp::AllocTexture<Texture>();
    _normalRT                 = PlayApp::AllocTexture<Texture>();
    _accumulateRT             = PlayApp::AllocTexture<Texture>();
    _postProcessRT            = PlayApp::AllocTexture<Texture>();
    auto colorImageCreateInfo = nvvk::makeImage2DCreateInfo(
        _app->getSize(), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT, false);
    auto samplerCreateInfo = nvvk::makeSamplerCreateInfo();
    {
        // depth image creation
        auto depthImageCreateInfo = nvvk::makeImage2DCreateInfo(
            _app->getSize(), VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false);
        vkCreateImage(_app->m_device, &depthImageCreateInfo, nullptr, &_depthRT.image);
        // Allocate the memory
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(_app->m_device, _depthRT.image, &memReqs);
        VkMemoryAllocateInfo memAllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex =
            _app->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(_app->m_device, &memAllocInfo, nullptr, &_depthRT.memory);
        // Bind image and memory
        vkBindImageMemory(_app->m_device, _depthRT.image, _depthRT.memory, 0);
        cmd = _app->createTempCmdBuffer();

        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;

        VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageMemoryBarrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        imageMemoryBarrier.image                 = _depthRT.image;
        imageMemoryBarrier.subresourceRange      = subresourceRange;
        imageMemoryBarrier.srcAccessMask         = VkAccessFlags();
        imageMemoryBarrier.dstAccessMask         = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        const VkPipelineStageFlags srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        const VkPipelineStageFlags destStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        vkCmdPipelineBarrier(cmd, srcStageMask, destStageMask, VK_FALSE, 0, nullptr, 0, nullptr, 1,
                             &imageMemoryBarrier);

        _app->submitTempCmdBuffer(cmd);
        VkImageViewCreateInfo depthViewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        depthViewCreateInfo.image            = _depthRT.image;
        depthViewCreateInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        depthViewCreateInfo.format           = depthImageCreateInfo.format;
        depthViewCreateInfo.subresourceRange = subresourceRange;
        _depthRT.layout                      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        vkCreateImageView(_app->m_device, &depthViewCreateInfo, nullptr, &_depthRT.view);
    }
    cmd = _app->createTempCmdBuffer();
    std::function<void(Texture*, VkImageCreateInfo&)> createTexture =
        [&](Texture* texture, VkImageCreateInfo& imgInfo)
    {
        nvvk::Texture nvvkTexture = _app->_alloc.createTexture(
            cmd, 0, nullptr, imgInfo, samplerCreateInfo, VK_IMAGE_LAYOUT_UNDEFINED);
        texture->image      = nvvkTexture.image;
        texture->memHandle  = nvvkTexture.memHandle;
        texture->descriptor = nvvkTexture.descriptor;
        texture->_format    = imgInfo.format;
        CUSTOM_NAME_VK(_app->m_debug, texture->image);
    };
    createTexture(_diffuseRT, colorImageCreateInfo);
    createTexture(_specularRT, colorImageCreateInfo);
    createTexture(_radianceRT, colorImageCreateInfo);
    createTexture(_normalRT, colorImageCreateInfo);

    auto accumulateImageCreateInfo = nvvk::makeImage2DCreateInfo(
        _app->getSize(), VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        false);
    createTexture(_accumulateRT, accumulateImageCreateInfo);
    auto postProcessImageCreateInfo = nvvk::makeImage2DCreateInfo(
        _app->getSize(), VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        false);
    createTexture(_postProcessRT, postProcessImageCreateInfo);
    _app->submitTempCmdBuffer(cmd);
}
} // namespace Play
