#include "VolumeRenderer.h"
#include "json.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/compute_vk.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvh/geometry.hpp"
#include "utils.hpp"
#include <PlayApp.h>
#include "nvh/misc.hpp"
#include "stb_image.h"
#include "host_device.h"
#include "debugger/debugger.h"
namespace Play
{
struct VolumeTexture : public Texture
{
    VkExtent3D _extent = {0, 0, 0};
};

VolumeRenderer::VolumeRenderer(PlayApp& app)
{
    _app = &app;
    for (int i = 0; i < _textureSlot.size(); ++i)
    {
        _textureSlot[i] = PlayApp::AllocTexture<Texture>();
    }
    // create volume texture
    loadVolumeTexture("content/volumeData/Textures/manix.dat");
    createRenderResource();
    createComputePasses();
    _renderUniformBuffer = PlayApp::AllocBuffer<Buffer>();
    VkBufferCreateInfo bufferInfo =
        nvvk::makeBufferCreateInfo(sizeof(RenderUniform), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR);
    auto nvvkBuffer = _app->_alloc.createBuffer(
        bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    _renderUniformBuffer->buffer = nvvkBuffer.buffer;
    _renderUniformBuffer->address = nvvkBuffer.address;
    _renderUniformBuffer->memHandle = nvvkBuffer.memHandle;
    _renderUniformBuffer->descriptor.buffer = nvvkBuffer.buffer;
    _renderUniformBuffer->descriptor.offset = 0;
    _renderUniformBuffer->descriptor.range  = sizeof(RenderUniform);
    CUSTOM_NAME_VK(_app->m_debug, _renderUniformBuffer->buffer);

}

ComputePass::ComputePass(PlayApp* app)
{
    this->app = app;
    this->dispatcher.init(app->getDevice());
}

ComputePass& ComputePass::addInputTexture(Texture* texture)
{
    inputTextures.push_back(texture);
    return *this;
}
ComputePass&         ComputePass::addInputBuffer(Buffer* buffer,VkDescriptorType type)
{
    inputBuffers.push_back(buffer);
    bufferTypes.push_back(type);
    return *this;
}

ComputePass& ComputePass::addComponent(Texture* texture, VkImageLayout initLayout,VkImageLayout finalLayout, bool needClear)
{
    assert(texture!=nullptr);
    this->outputComponent.push_back(texture);
    this->layoutStates.push_back({initLayout,finalLayout,needClear});
    return *this;
}

void ComputePass:: setShaderCode(const std::string& filename)
{
    auto shaderCode = nvh::loadFile(filename, true);
    dispatcher.setCode(shaderCode.data(), shaderCode.size());
}

void ComputePass::build(PlayApp* _app)
{
    
    for(int i = 0;i<inputTextures.size();++i)
    {
        dispatcher.getBindings().addBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                            VK_SHADER_STAGE_COMPUTE_BIT);
    }
    for (int i = 0; i < inputBuffers.size(); ++i)
    {
       
        dispatcher.getBindings().addBinding(static_cast<uint32_t>(inputTextures.size()) + static_cast<uint32_t>(i),
                                            bufferTypes[i], 1,VK_SHADER_STAGE_COMPUTE_BIT);
    }
    for (int i = 0; i < outputComponent.size(); ++i)
    {
        dispatcher.getBindings().addBinding(static_cast<uint32_t>(inputTextures.size()) + static_cast<uint32_t>(inputBuffers.size()) + static_cast<uint32_t>(i),
                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,VK_SHADER_STAGE_COMPUTE_BIT);
    }
    dispatcher.finalizePipeline();
}
void ComputePass::beginPass(VkCommandBuffer cmd)
{
    std::vector<VkImageMemoryBarrier> imageMemoryBarriers;
    for (int i = 0; i < inputTextures.size(); ++i)
    {
        VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageMemoryBarrier.image = inputTextures[i]->image;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.srcAccessMask               = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        imageMemoryBarrier.srcQueueFamilyIndex = app->getQueueFamily();
        imageMemoryBarrier.dstQueueFamilyIndex = app->getQueueFamily();
        imageMemoryBarriers.push_back(imageMemoryBarrier);
    }
    for (int j = 0; j < outputComponent.size(); ++j)
    {
        VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageMemoryBarrier.image = outputComponent[j]->image;
        imageMemoryBarrier.oldLayout = layoutStates[j].initlayout;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.srcAccessMask               = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        imageMemoryBarrier.srcQueueFamilyIndex = app->getQueueFamily();
        imageMemoryBarrier.dstQueueFamilyIndex = app->getQueueFamily();
        imageMemoryBarriers.push_back(imageMemoryBarrier);
    }
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, imageMemoryBarriers.size(), imageMemoryBarriers.data());
}
void ComputePass::endPass(VkCommandBuffer cmd)
{
    
}
void ComputePass::dispatch(VkCommandBuffer cmd, uint32_t width, uint32_t height, uint32_t depth)
{
    for(int i = 0;i<inputTextures.size();++i)
    {
        auto& texture = inputTextures[i];
        if(texture!=nullptr)
        {
            dispatcher.updateBinding(i,texture->descriptor.imageView,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,texture->descriptor.sampler);
        }
    }
    for (int i = 0; i < inputBuffers.size(); ++i)
    {
        auto& buffer = inputBuffers[i];
        if (buffer != nullptr)
        {
            dispatcher.updateBinding(static_cast<uint32_t>(inputTextures.size())+static_cast<uint32_t>(i), buffer->descriptor.buffer);
        }
    }
    for (int i = 0; i < outputComponent.size(); ++i)
    {
        auto& texture = outputComponent[i];
        if (texture != nullptr)
        {
            dispatcher.updateBinding(static_cast<uint32_t>(inputTextures.size()) + static_cast<uint32_t>(inputBuffers.size()) + static_cast<uint32_t>(i), texture->descriptor.imageView,VK_IMAGE_LAYOUT_GENERAL, texture->descriptor.sampler);
        }
    }
    dispatcher.bind(cmd, nullptr, 0);
    vkCmdDispatch(cmd, width, height, depth);
}
void ComputePass::destroy()
{
    this->dispatcher.deinit();

}
void VolumeRenderer::createComputePasses()
{
    _generateRaysPass = new ComputePass(_app);
    _radiancesPass = new ComputePass(_app);
    _accumulatePass = new ComputePass(_app);
    _postProcessPass  = new ComputePass(_app);
    // generate rays pass
    _generateRaysPass->addInputTexture(_textureSlot[eVolumeTexture]).addInputTexture(_textureSlot[eGradientTexture])
    .addInputTexture(_textureSlot[eDiffuseLookUpTexture]).addInputTexture(_textureSlot[eSpecularLookUpTexture])
    .addInputTexture(_textureSlot[eRoughnessLookUpTexture]).addInputTexture(_textureSlot[eOpacityLookUpTexture]);
    _generateRaysPass->addComponent(_textureSlot[eDiffuseRT],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    .addComponent(_textureSlot[eSpecularRT],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    .addComponent(_textureSlot[eNormalRT],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    .addComponent(_textureSlot[eDepthRT],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _generateRaysPass->setShaderCode( "spv/volumeGenRay.comp.spv");
    _generateRaysPass->build(_app);
    // radiance pass
    _radiancesPass->addInputTexture(_textureSlot[eVolumeTexture]).addInputTexture(_textureSlot[eOpacityLookUpTexture]).addInputTexture(_textureSlot[eDiffuseRT])
    .addInputTexture(_textureSlot[eSpecularRT]).addInputTexture(_textureSlot[eNormalRT]).addInputTexture(_textureSlot[eDepthRT]).addInputTexture(_textureSlot[eEnvTexture]);
    _radiancesPass->addComponent(_textureSlot[eRadianceRT],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _radiancesPass->setShaderCode("spv/volumeRadiance.comp.spv");
    _radiancesPass->build(_app);
    // accumulate pass
    _accumulatePass->addInputTexture(_textureSlot[eRadianceRT]);
    _accumulatePass->addComponent(_textureSlot[eAccumulateRT],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,false);
    _accumulatePass->setShaderCode("spv/volumeAccumulate.comp.spv");
    _accumulatePass->build(_app);
    // postprocess pass
    _postProcessPass->addInputTexture(_textureSlot[eAccumulateRT]);
    _postProcessPass->addComponent(_textureSlot[ePostProcessRT],VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _postProcessPass->setShaderCode("spv/volumePostProcess.comp.spv");
    _postProcessPass->build(_app);
}
void VolumeRenderer::OnPreRender()
{
    VolumeRenderUniformBuffer* data = static_cast<VolumeRenderUniformBuffer*>(PlayApp::MapBuffer(*_renderUniformBuffer));
    data->viewMatrix          = CameraManip.getMatrix();
    data->inverseViewMatrix   = glm::inverse(data->viewMatrix);
    data->projMatrix       = glm::perspectiveFov(CameraManip.getFov(), _app->getSize().width * 1.0f,
                                              _app->getSize().height * 1.0f, 0.1f, 10000.0f);
    data->projMatrix[1][1] *= -1;
    data->cameraPos = CameraManip.getEye();
    data->frameCount     = _frameCount;
    PlayApp::UnmapBuffer(*_renderUniformBuffer);
    if (_dirtyCamera != CameraManip.getCamera())
    {
        _frameCount  = 0;
        _dirtyCamera = CameraManip.getCamera();
    }
}
void VolumeRenderer::OnPostRender() {}
void VolumeRenderer::RenderFrame()
{
    ivec2 DispatchSize = {static_cast<uint32_t>(std::ceil(_app->getSize().width/8.0f)), static_cast<uint32_t>(std::ceil(_app->getSize().height/8.0f))};
    // begin generate rays pass
    auto cmd = _app->getCommandBuffers()[_app->m_swapChain.getActiveImageIndex()];
    _generateRaysPass->beginPass(cmd);
    _generateRaysPass->dispatch(cmd, DispatchSize.x, DispatchSize.y, 1);
    _generateRaysPass->endPass(cmd);
    // begin radiance pass
    _radiancesPass->beginPass(cmd);
    _radiancesPass->dispatch(cmd, DispatchSize.x, DispatchSize.y, 1);
    _radiancesPass->endPass(cmd);
    // begin accumulate pass
    _accumulatePass->beginPass(cmd);
    _accumulatePass->dispatch(cmd, DispatchSize.x, DispatchSize.y, 1);
    _accumulatePass->endPass(cmd);
    // begin post process pass
    _postProcessPass->beginPass(cmd);
    _postProcessPass->dispatch(cmd, DispatchSize.x,DispatchSize.y, 1);
    _postProcessPass->endPass(cmd);
    // end post process pass
    {
        // switch _postprocess pass render target image layout
        VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageMemoryBarrier.image = _textureSlot[ePostProcessRT]->image;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.srcQueueFamilyIndex = _app->getQueueFamily();
        imageMemoryBarrier.dstQueueFamilyIndex = _app->getQueueFamily();
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        
    }

}
void VolumeRenderer::SetScene(Scene* scene) {}
void VolumeRenderer::OnResize(int width, int height) {}
void VolumeRenderer::OnDestroy()
{
    delete _generateRaysPass;
    delete _radiancesPass;
    delete _accumulatePass;
    delete _postProcessPass;
    delete _gradiantPass;
}
void VolumeRenderer::loadVolumeTexture(const std::string& filename)
{
    std::unique_ptr<FILE, decltype(&fclose)> file(fopen(filename.c_str(), "rb"), fclose);
    if (!file)
    {
        throw std::runtime_error("Failed to open volume data file.");
    }

    _textureSlot[uint32_t(TextureBinding::eVolumeTexture)] =
        static_cast<VolumeTexture*>(PlayApp::AllocTexture<VolumeTexture>());
    _textureSlot[uint32_t(TextureBinding::eGradientTexture)] =
        static_cast<VolumeTexture*>(PlayApp::AllocTexture<VolumeTexture>());
    auto _volumeTexture =
        static_cast<VolumeTexture*>(_textureSlot[uint32_t(TextureBinding::eVolumeTexture)]);
    auto _gradiantTexture =_textureSlot[uint32_t(TextureBinding::eGradientTexture)];
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
    imageCreateInfo.mipLevels     = 1;
    imageCreateInfo.flags         = 0;
    imageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.pNext         = nullptr;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto          cmd               = _app->createTempCmdBuffer();
    nvvk::Texture nvvkVolumeTexture = _app->_alloc.createTexture(
        cmd, intensityData.size() * sizeof(uint16_t), intensityData.data(), imageCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    nvvk::Texture nvvkGradiantTexture = _app->_alloc.createTexture(
        cmd, 0, nullptr, imageCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_UNDEFINED);
    _app->submitTempCmdBuffer(cmd);
    _volumeTexture->image = nvvkVolumeTexture.image;
    _volumeTexture->memHandle = nvvkVolumeTexture.memHandle;
    _volumeTexture->descriptor = nvvkVolumeTexture.descriptor;
    _volumeTexture->_format    = imageCreateInfo.format;
    _volumeTexture->_mipmapLevel = imageCreateInfo.mipLevels;
    CUSTOM_NAME_VK(_app->m_debug, _volumeTexture->image);
    _gradiantTexture->image = nvvkGradiantTexture.image;
    _gradiantTexture->memHandle = nvvkGradiantTexture.memHandle;
    _gradiantTexture->descriptor = nvvkGradiantTexture.descriptor;
    _gradiantTexture->_format    = imageCreateInfo.format;
    _gradiantTexture->_mipmapLevel = imageCreateInfo.mipLevels;
    CUSTOM_NAME_VK(_app->m_debug, _gradiantTexture->image);

    // gradiant pass
    _gradiantPass = new ComputePass(_app);
   _gradiantPass->addInputTexture(_textureSlot[eVolumeTexture]);
   _gradiantPass->addComponent(_textureSlot[eGradientTexture], VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
   _gradiantPass->setShaderCode("spv/gradiant.comp.spv");
   _gradiantPass->build(_app);
   // prepare gradiant texture
    cmd = _app->createTempCmdBuffer();
    _gradiantPass->beginPass(cmd);
    const auto threadGroupX = static_cast<uint32_t>(std::ceil(_volumeTexture->_extent.width / 8.0f));
    const auto threadGroupY = static_cast<uint32_t>(std::ceil(_volumeTexture->_extent.height / 8.0f));
    const auto threadGroupZ = static_cast<uint32_t>(std::ceil(_volumeTexture->_extent.depth / 8.0f));
    _gradiantPass->dispatch(cmd, threadGroupX, threadGroupY,threadGroupZ);
    _gradiantPass->endPass(cmd);
    _app->submitTempCmdBuffer(cmd);
    _gradiantPass->destroy();
}
void VolumeRenderer::createRenderResource()
{
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
    _textureSlot[eDiffuseLookUpTexture] = PlayApp::AllocTexture<Texture>();
    _textureSlot[eSpecularLookUpTexture] = PlayApp::AllocTexture<Texture>();
    _textureSlot[eRoughnessLookUpTexture] = PlayApp::AllocTexture<Texture>();
    _textureSlot[eOpacityLookUpTexture] = PlayApp::AllocTexture<Texture>();

    auto          cmd                      = _app->createTempCmdBuffer();
    nvvk::Texture nvvkDiffuseLookUpTexture = _app->_alloc.createTexture(
        cmd, diffuseData.size() * sizeof(glm::vec4), diffuseData.data(), image2DCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        _textureSlot[eDiffuseLookUpTexture]->image      = nvvkDiffuseLookUpTexture.image;
        _textureSlot[eDiffuseLookUpTexture]->memHandle  = nvvkDiffuseLookUpTexture.memHandle;
        _textureSlot[eDiffuseLookUpTexture]->descriptor = nvvkDiffuseLookUpTexture.descriptor;
        _textureSlot[eDiffuseLookUpTexture]->_format    = image2DCreateInfo.format;
    CUSTOM_NAME_VK(_app->m_debug, _textureSlot[eDiffuseLookUpTexture]->image);
    nvvk::Texture nvvkSpecularLookUpTexture = _app->_alloc.createTexture(
        cmd, specularData.size() * sizeof(glm::vec4), specularData.data(), image2DCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        _textureSlot[eSpecularLookUpTexture]->image      = nvvkSpecularLookUpTexture.image;
        _textureSlot[eSpecularLookUpTexture]->memHandle  = nvvkSpecularLookUpTexture.memHandle;
        _textureSlot[eSpecularLookUpTexture]->descriptor = nvvkSpecularLookUpTexture.descriptor;
        _textureSlot[eSpecularLookUpTexture]->_format    = image2DCreateInfo.format;
    CUSTOM_NAME_VK(_app->m_debug, _textureSlot[eSpecularLookUpTexture]->image);
    nvvk::Texture nvvkRoughnessLoopUpTexture = _app->_alloc.createTexture(
        cmd, roughnessData.size() * sizeof(uint8_t), roughnessData.data(), image1DCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        _textureSlot[eRoughnessLookUpTexture]->image      = nvvkRoughnessLoopUpTexture.image;
        _textureSlot[eRoughnessLookUpTexture]->memHandle  = nvvkRoughnessLoopUpTexture.memHandle;
        _textureSlot[eRoughnessLookUpTexture]->descriptor = nvvkRoughnessLoopUpTexture.descriptor;
        _textureSlot[eRoughnessLookUpTexture]->_format    = image1DCreateInfo.format;
    CUSTOM_NAME_VK(_app->m_debug,  _textureSlot[eRoughnessLookUpTexture]->image);
    nvvk::Texture nvvkOpacityLookUpTexture = _app->_alloc.createTexture(
        cmd, opacityData.size() * sizeof(uint8_t), opacityData.data(), image1DCreateInfo,
        nvvk::makeSamplerCreateInfo(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        _textureSlot[eOpacityLookUpTexture]->image      = nvvkOpacityLookUpTexture.image;
        _textureSlot[eOpacityLookUpTexture]->memHandle  = nvvkOpacityLookUpTexture.memHandle;
        _textureSlot[eOpacityLookUpTexture]->descriptor = nvvkOpacityLookUpTexture.descriptor;
        _textureSlot[eOpacityLookUpTexture]->_format    = image1DCreateInfo.format;
    CUSTOM_NAME_VK(_app->m_debug,  _textureSlot[eOpacityLookUpTexture]->image);
    _app->submitTempCmdBuffer(cmd);

    // create multiple render target

    _textureSlot[eDiffuseRT]                = PlayApp::AllocTexture<Texture>();
    _textureSlot[eSpecularRT]               = PlayApp::AllocTexture<Texture>();
    _textureSlot[eRadianceRT]               = PlayApp::AllocTexture<Texture>();
    _textureSlot[eNormalRT]                 = PlayApp::AllocTexture<Texture>();
    _textureSlot[eAccumulateRT]             = PlayApp::AllocTexture<Texture>();
    _textureSlot[ePostProcessRT]            = PlayApp::AllocTexture<Texture>();
    auto colorImageCreateInfo = nvvk::makeImage2DCreateInfo(
        _app->getSize(), VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT, false);
    auto samplerCreateInfo = nvvk::makeSamplerCreateInfo();
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
        // CUSTOM_NAME_VK(_app->m_debug, nvvkTexture.image);
    };
    createTexture(_textureSlot[eDiffuseRT], colorImageCreateInfo);
    createTexture(_textureSlot[eSpecularRT], colorImageCreateInfo);
    CUSTOM_NAME_VK(_app->m_debug, _textureSlot[eSpecularRT]->image);
    createTexture(_textureSlot[eRadianceRT], colorImageCreateInfo);
    CUSTOM_NAME_VK(_app->m_debug, _textureSlot[eRadianceRT]->image);
    createTexture(_textureSlot[eNormalRT], colorImageCreateInfo);
    CUSTOM_NAME_VK(_app->m_debug, _textureSlot[eNormalRT]->image);
    {
        // depth image creation
        _textureSlot[eDepthRT] = PlayApp::AllocTexture<Texture>();
        auto depthImageCreateInfo = nvvk::makeImage2DCreateInfo(
            _app->getSize(), VK_FORMAT_R32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false);
        createTexture(_textureSlot[eDepthRT], depthImageCreateInfo);
    }
    // load env texture
    std::string path = "D:\\repo\\DogEngine\\models\\skybox\\graveyard_pathways_2k.hdr";
    int    width, height, channels;
    float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
    if (!data)
    {
        throw std::runtime_error("load env texture failed");
    }
    VkExtent2D        size{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    VkImageCreateInfo imageCreateInfo = nvvk::makeImage2DCreateInfo(
        size, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);
    _textureSlot[eEnvTexture]       = PlayApp::AllocTexture<Texture>();
    createTexture(_textureSlot[eEnvTexture], imageCreateInfo);

    //create accumulate texture
    auto accumulateImageCreateInfo = nvvk::makeImage2DCreateInfo(
        _app->getSize(), VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        false);
    createTexture(_textureSlot[eAccumulateRT], accumulateImageCreateInfo);
    auto postProcessImageCreateInfo = nvvk::makeImage2DCreateInfo(
        _app->getSize(), VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        false);
    createTexture(_textureSlot[ePostProcessRT], postProcessImageCreateInfo);
    _app->submitTempCmdBuffer(cmd);
    assert(checkResourceState() && "Some crutial resource is not created successfully!");
}

bool VolumeRenderer::checkResourceState()
{
    for (auto& texture : _textureSlot)
    {
        if (!texture) return false;
    }
    return true;
}
}// namespace Play
