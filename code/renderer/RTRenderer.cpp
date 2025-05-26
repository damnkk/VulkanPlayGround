#include "RTRenderer.h"
#include "resourceManagement/SceneNode.h"
#include "PlayApp.h"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvvk/shadermodulemanager_vk.hpp"
#include "nvvk/shaders_vk.hpp"
#include "utils.hpp"
#include "stb_image.h"
namespace Play
{
RTRenderer::RTRenderer(PlayApp& app)
{
    _app = &app;
    _rtBuilder.setup(app.m_device, &app._alloc, app.getQueueFamily());
    _rtBuilder.setup(app.m_device, &app._alloc, app.m_graphicsQueueIndex);
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    properties2.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(app.m_physicalDevice, &properties2);
    _shaderModuleManager.init(app.m_device, 1, 3);
    _sbtWrapper.setup(app.m_device, app.getQueueFamily(), &app._alloc, rtProperties);
    rayTraceRTCreate();
    loadEnvTexture();
    createRenderBuffer();
    SetScene(&_app->_scene);
    createRTPipeline();
    createPostProcessResource();

}
void RTRenderer::OnPreRender()
{
    RenderUniform* data = static_cast<RenderUniform*>(PlayApp::MapBuffer(*_renderUniformBuffer));
    data->view          = CameraManip.getMatrix();
    data->viewInverse   = glm::inverse(data->view);
    data->project       = glm::perspectiveFov(CameraManip.getFov(), _app->getSize().width * 1.0f,
                                              _app->getSize().height * 1.0f, 0.1f, 10000.0f);
    data->project[1][1] *= -1;
    data->cameraPosition = CameraManip.getEye();
    data->frameCount     = _frameCount;
    PlayApp::UnmapBuffer(*_renderUniformBuffer);
    if (_dirtyCamera != CameraManip.getCamera())
    {
        _frameCount  = 0;
        _dirtyCamera = CameraManip.getCamera();
    }
}
void RTRenderer::OnPostRender()
{
    auto cmd = _app->getCommandBuffers()[_app->m_swapChain.getActiveImageIndex()];
    VkClearValue clearValue[1];
    clearValue[0].color        = {{0.0f, 1.0f, 1.0f, 1.0f}};
    VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBeginInfo.renderPass = _postProcessRenderPass;
    renderPassBeginInfo.framebuffer = _postProcessFBO;
    renderPassBeginInfo.renderArea      = VkRect2D({0, 0}, _app->getSize());
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues    = clearValue;
    vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _postPipeline);
    vkCmdSetDepthTestEnable(cmd, true);
    vkCmdSetDepthWriteEnable(cmd, true);
    VkViewport viewport = {
        0.0f, 0.0f, (float) _app->getSize().width, (float) _app->getSize().height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {{0, 0}, _app->getSize()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _postPipelineLayout, 0, 1,
                            &_postDescriptorSet, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}
void RTRenderer::RenderFrame()
{
    auto                 cmd = _app->getCommandBuffers()[_app->m_swapChain.getActiveImageIndex()];
    VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout                   = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.image                       = _rayTraceRT->image;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.srcAccessMask               = 0;
    imageMemoryBarrier.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = _app->getQueueFamily();
    imageMemoryBarrier.dstQueueFamilyIndex = _app->getQueueFamily();
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrier);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rtPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rtPipelineLayout, 0, 1,
                            &_descriptorSet, 0, nullptr);
    auto regions = _sbtWrapper.getRegions();
    vkCmdTraceRaysKHR(cmd, &regions[0], &regions[1], &regions[2], &regions[3],
                      _app->getSize().width, _app->getSize().height, 1);

    imageMemoryBarrier.oldLayout                   = VK_IMAGE_LAYOUT_GENERAL;
    imageMemoryBarrier.newLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageMemoryBarrier.image                       = _rayTraceRT->image;
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageMemoryBarrier.subresourceRange.levelCount = 1;
    imageMemoryBarrier.subresourceRange.layerCount = 1;
    imageMemoryBarrier.srcAccessMask               = 0;
    imageMemoryBarrier.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    imageMemoryBarrier.srcQueueFamilyIndex = _app->getQueueFamily();
    imageMemoryBarrier.dstQueueFamilyIndex = _app->getQueueFamily();
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrier);
    ++_frameCount;
}
void RTRenderer::SetScene(Scene* scene)
{
    this->_scene = scene;
    buildBlas();
    buildTlas();
    createDescritorSet();
}

void RTRenderer::loadEnvTexture()
{
    std::string path = ".\\resource\\skybox\\graveyard_pathways_2k.hdr";
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
    VkSamplerCreateInfo samplerCreateInfo = nvvk::makeSamplerCreateInfo(
        VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE,
        1.0f, VK_SAMPLER_MIPMAP_MODE_LINEAR);
    _envTexture       = PlayApp::AllocTexture<Texture>();
    auto          cmd = _app->createTempCmdBuffer();
    nvvk::Texture nvvkTexture =
        _app->_alloc.createTexture(cmd, width * height * 4 * sizeof(float), data, imageCreateInfo,
                                   samplerCreateInfo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    nvvk::cmdGenerateMipmaps(cmd, nvvkTexture.image, imageCreateInfo.format,
                             {imageCreateInfo.extent.width, imageCreateInfo.extent.height},
                             imageCreateInfo.mipLevels);
    _app->submitTempCmdBuffer(cmd);

    _envTexture->image        = nvvkTexture.image;
    _envTexture->memHandle    = nvvkTexture.memHandle;
    _envTexture->descriptor   = nvvkTexture.descriptor;
    _envTexture->_format      = imageCreateInfo.format;
    _envTexture->_mipmapLevel = 1;

    // build accel
    float lumSum = 0.0;

    // 初始化 h 行 w 列的概率密度 pdf 并 统计总亮度
    std::vector<std::vector<float>> pdf(height);
    for (auto& line : pdf) line.resize(width);
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            float R   = data[4 * (i * width + j)];
            float G   = data[4 * (i * width + j) + 1];
            float B   = data[4 * (i * width + j) + 2];
            float lum = 0.2f * R + 0.7f * G + 0.1f * B;
            pdf[i][j] = lum;
            lumSum += lum;
        }
    }
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++) pdf[i][j] /= lumSum;
    std::vector<float> pdf_x_margin;
    pdf_x_margin.resize(width);
    for (int j = 0; j < width; j++)
        for (int i = 0; i < height; i++) pdf_x_margin[j] += pdf[i][j];
    std::vector<float> cdf_x_margin = pdf_x_margin;
    for (int i = 1; i < width; i++) cdf_x_margin[i] += cdf_x_margin[i - 1];

    std::vector<std::vector<float>> pdf_y_condiciton = pdf;
    for (int j = 0; j < width; j++)
        for (int i = 0; i < height; i++) pdf_y_condiciton[i][j] /= pdf_x_margin[j];

    std::vector<std::vector<float>> cdf_y_condiciton = pdf_y_condiciton;
    for (int j = 0; j < width; j++)
        for (int i = 1; i < height; i++) cdf_y_condiciton[i][j] += cdf_y_condiciton[i - 1][j];

    std::vector<std::vector<float>> temp = cdf_y_condiciton;
    cdf_y_condiciton                     = std::vector<std::vector<float>>(width);
    for (auto& line : cdf_y_condiciton) line.resize(height);
    for (int j = 0; j < width; j++)
        for (int i = 0; i < height; i++) cdf_y_condiciton[j][i] = temp[i][j];
    std::vector<std::vector<float>> sample_x(height);
    for (auto& line : sample_x) line.resize(width);
    std::vector<std::vector<float>> sample_y(height);
    for (auto& line : sample_y) line.resize(width);
    std::vector<std::vector<float>> sample_p(height);
    for (auto& line : sample_p) line.resize(width);
    for (int j = 0; j < width; j++)
    {
        for (int i = 0; i < height; i++)
        {
            float xi_1 = float(i) / height;
            float xi_2 = float(j) / width;
            int   x    = std::lower_bound(cdf_x_margin.begin(), cdf_x_margin.end(), xi_1) -
                    cdf_x_margin.begin();
            x     = fmin(x, width - 1);
            int y = std::lower_bound(cdf_y_condiciton[x].begin(), cdf_y_condiciton[x].end(), xi_2) -
                    cdf_y_condiciton[x].begin();
            sample_x[i][j] = float(x) / width;
            sample_y[i][j] = float(y) / height;
            sample_p[i][j] = pdf[i][j];
        }
    }

    std::vector<float> cache(width * height * 4);
    // for (int i = 0; i < width * height * 3; i++) cache[i] = 0.0;

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            cache[4 * (i * width + j)]     = sample_x[i][j]; // R
            cache[4 * (i * width + j) + 1] = sample_y[i][j]; // G
            cache[4 * (i * width + j) + 2] = sample_p[i][j]; // B
            cache[4 * (i * width + j) + 3] = 1.0;            // A
        }
    }

    VkExtent2D        size2{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    VkImageCreateInfo imageCreateInfo2 = nvvk::makeImage2DCreateInfo(
        size2, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false);
    VkSamplerCreateInfo samplerCreateInfo2 = nvvk::makeSamplerCreateInfo(
        VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE,
        1.0f, VK_SAMPLER_MIPMAP_MODE_LINEAR);
    _envLookupTexture          = PlayApp::AllocTexture<Texture>();
    auto          cmd2         = _app->createTempCmdBuffer();
    nvvk::Texture nvvkTexture2 = _app->_alloc.createTexture(
        cmd2, width * height * 4 * sizeof(float), cache.data(), imageCreateInfo2,
        samplerCreateInfo2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _envLookupTexture->image        = nvvkTexture2.image;
    _envLookupTexture->memHandle    = nvvkTexture2.memHandle;
    _envLookupTexture->descriptor   = nvvkTexture2.descriptor;
    _envLookupTexture->_format      = imageCreateInfo2.format;
    _envLookupTexture->_mipmapLevel = imageCreateInfo2.mipLevels;
    CUSTOM_NAME_VK(_app->m_debug, _envLookupTexture->image);
    _app->submitTempCmdBuffer(cmd2);
    stbi_image_free(data);
}
void RTRenderer::buildTlas()
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    auto                                            meshes = _app->_modelLoader.getSceneMeshes();
    std::queue<SceneNode*>                          nodes;
    nodes.push(this->_scene->_root.get());
    while (!nodes.empty())
    {
        auto node = nodes.front();
        nodes.pop();

        if (!node->_meshIdx.empty())
        {
            for (int i = 0; i < node->_meshIdx.size(); ++i)
            {
                VkAccelerationStructureInstanceKHR instance{};
                instance.transform           = nvvk::toTransformMatrixKHR(node->_transform);
                instance.instanceCustomIndex = node->_meshIdx[i];
                instance.accelerationStructureReference =
                    _rtBuilder.getBlasDeviceAddress(node->_meshIdx[i]);
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                instance.mask  = 0xFF;
                instance.instanceShaderBindingTableRecordOffset = 0;
                instances.push_back(instance);
            }
        }
        for (auto& child : node->_children)
        {
            nodes.push(child.get());
        }
    }
    assert(instances.size());
    _rtBuilder.buildTlas(instances);
    _tlasAccels = _rtBuilder.getAccelerationStructure();
}
void RTRenderer::buildBlas()
{
    std::vector<nvvk::RaytracingBuilderKHR::BlasInput> allBlas;
    allBlas.reserve(_app->_modelLoader.getSceneMeshes().size());
    std::vector<Mesh>& meshes = _app->_modelLoader.getSceneMeshes();
    for (int b = 0; b < meshes.size(); ++b)
    {
        Mesh& mesh = meshes[b];

        VkDeviceAddress                                 vertexAddress     = mesh._vertexAddress;
        VkDeviceAddress                                 indexAddress      = mesh._indexAddress;
        uint32_t                                        maxPrimitiveCount = mesh._faceCnt;
        VkAccelerationStructureGeometryTrianglesDataKHR triangles{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; // vec3 vertex position data.
        triangles.vertexData.deviceAddress = vertexAddress;
        triangles.vertexStride             = sizeof(Vertex);
        // Describe index data (32-bit unsigned int)
        triangles.indexType               = VK_INDEX_TYPE_UINT32;
        triangles.indexData.deviceAddress = indexAddress;
        // Indicate identity transform by setting transformData to null device pointer.
        // triangles.transformData = {};
        triangles.maxVertex = mesh._vertCnt - 1;

        // Identify the above data as containing opaque triangles.
        VkAccelerationStructureGeometryKHR asGeom{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
        asGeom.geometry.triangles = triangles;

        VkAccelerationStructureBuildRangeInfoKHR offset;
        offset.firstVertex     = 0;
        offset.primitiveCount  = maxPrimitiveCount;
        offset.primitiveOffset = 0;
        offset.transformOffset = 0;

        nvvk::RaytracingBuilderKHR::BlasInput input;
        input.asGeometry.emplace_back(asGeom);
        input.asBuildOffsetInfo.emplace_back(offset);
        allBlas.push_back(input);
    };
    _rtBuilder.buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}
void RTRenderer::createDescritorSet()
{
    // tlas desc binding
    VkDescriptorSetLayoutBinding tlasLayoutBinding;
    tlasLayoutBinding.binding         = ObjBinding::eTlas;
    tlasLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    tlasLayoutBinding.descriptorCount = 1;
    tlasLayoutBinding.stageFlags =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
    // raytracing RT desc binding
    VkDescriptorSetLayoutBinding rayTraceRTLayoutBinding;
    rayTraceRTLayoutBinding.binding         = ObjBinding::eRayTraceRT;
    rayTraceRTLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rayTraceRTLayoutBinding.descriptorCount = 1;
    rayTraceRTLayoutBinding.stageFlags =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT;
    // material buffer desc binding
    VkDescriptorSetLayoutBinding materialBufferLayoutBinding;
    materialBufferLayoutBinding.binding         = ObjBinding::eMaterialBuffer;
    materialBufferLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialBufferLayoutBinding.descriptorCount = 1;
    materialBufferLayoutBinding.stageFlags =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    // render uniform buffer desc binding
    VkDescriptorSetLayoutBinding renderUniformBufferLayoutBinding;
    renderUniformBufferLayoutBinding.binding         = ObjBinding::eRenderUniform;
    renderUniformBufferLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    renderUniformBufferLayoutBinding.descriptorCount = 1;
    renderUniformBufferLayoutBinding.stageFlags =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    // light mesh idx buffer desc binding
    VkDescriptorSetLayoutBinding lightMeshIdxLayoutBinding;
    lightMeshIdxLayoutBinding.binding         = ObjBinding::eLightMeshIdx;
    lightMeshIdxLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    lightMeshIdxLayoutBinding.descriptorCount = 1;
    lightMeshIdxLayoutBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                           VK_SHADER_STAGE_MISS_BIT_KHR;
    // instance buffer desc binding
    VkDescriptorSetLayoutBinding instanceBufferLayoutBinding;
    instanceBufferLayoutBinding.binding         = ObjBinding::eInstanceBuffer;
    instanceBufferLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    instanceBufferLayoutBinding.descriptorCount = 1;
    instanceBufferLayoutBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                             VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                             VK_SHADER_STAGE_MISS_BIT_KHR;

    // scene texture desc binding
    VkDescriptorSetLayoutBinding SceneTextureLayoutBinding;
    SceneTextureLayoutBinding.binding        = ObjBinding::eSceneTexture;
    SceneTextureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    SceneTextureLayoutBinding.descriptorCount =
        static_cast<uint32_t>(_app->_modelLoader.getSceneTextures().size());
    SceneTextureLayoutBinding.stageFlags =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT;
    SceneTextureLayoutBinding.pImmutableSamplers = nullptr;
    // env texture desc binding
    VkDescriptorSetLayoutBinding envTextureLayoutBinding;
    envTextureLayoutBinding.binding            = ObjBinding::eEnvTexture;
    envTextureLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    envTextureLayoutBinding.descriptorCount    = 1;
    envTextureLayoutBinding.stageFlags         = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    envTextureLayoutBinding.pImmutableSamplers = nullptr;

    // env accel buffer desc binding
    VkDescriptorSetLayoutBinding envAccelBufferLayoutBinding;
    envAccelBufferLayoutBinding.binding            = ObjBinding::eEnvLoopupTexture;
    envAccelBufferLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    envAccelBufferLayoutBinding.descriptorCount    = 1;
    envAccelBufferLayoutBinding.stageFlags         = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    envAccelBufferLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        tlasLayoutBinding,           rayTraceRTLayoutBinding,
        materialBufferLayoutBinding, renderUniformBufferLayoutBinding,
        lightMeshIdxLayoutBinding,   instanceBufferLayoutBinding,
        SceneTextureLayoutBinding,   envTextureLayoutBinding,
        envAccelBufferLayoutBinding};
    VkDescriptorSetLayoutCreateInfo descSetLayoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    // descSetLayoutInfo.
    descSetLayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    descSetLayoutInfo.pBindings    = bindings.data();
    descSetLayoutInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    vkCreateDescriptorSetLayout(_app->m_device, &descSetLayoutInfo, nullptr, &_descriptorSetLayout);
    CUSTOM_NAME_VK(_app->m_debug, _descriptorSetLayout);
    VkDescriptorSetAllocateInfo descSetAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descSetAllocInfo.descriptorPool     = _app->_descriptorPool;
    descSetAllocInfo.descriptorSetCount = 1;
    descSetAllocInfo.pSetLayouts        = &_descriptorSetLayout;
    vkAllocateDescriptorSets(_app->m_device, &descSetAllocInfo, &_descriptorSet);
    CUSTOM_NAME_VK(_app->m_debug, _descriptorSet);

    VkWriteDescriptorSetAccelerationStructureKHR accWrite{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    accWrite.accelerationStructureCount = 1;
    accWrite.pAccelerationStructures    = &this->_tlasAccels;
    std::vector<VkWriteDescriptorSet> descSetWrites(ObjBinding::eCount,
                                                    {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET});
    descSetWrites[ObjBinding::eTlas].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    descSetWrites[ObjBinding::eTlas].dstBinding     = ObjBinding::eTlas;
    descSetWrites[ObjBinding::eTlas].dstSet         = _descriptorSet;
    descSetWrites[ObjBinding::eTlas].descriptorCount = 1;
    descSetWrites[ObjBinding::eTlas].pNext           = &accWrite;

    descSetWrites[ObjBinding::eRayTraceRT].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descSetWrites[ObjBinding::eRayTraceRT].dstBinding      = ObjBinding::eRayTraceRT;
    descSetWrites[ObjBinding::eRayTraceRT].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eRayTraceRT].descriptorCount = 1;
    descSetWrites[ObjBinding::eRayTraceRT].pImageInfo      = &_rayTraceRT->descriptor;

    descSetWrites[ObjBinding::eMaterialBuffer].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descSetWrites[ObjBinding::eMaterialBuffer].dstBinding      = ObjBinding::eMaterialBuffer;
    descSetWrites[ObjBinding::eMaterialBuffer].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eMaterialBuffer].descriptorCount = 1;
    descSetWrites[ObjBinding::eMaterialBuffer].pBufferInfo =
        &_app->_modelLoader.getMaterialBuffer()->descriptor;

    descSetWrites[ObjBinding::eRenderUniform].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descSetWrites[ObjBinding::eRenderUniform].dstBinding      = ObjBinding::eRenderUniform;
    descSetWrites[ObjBinding::eRenderUniform].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eRenderUniform].descriptorCount = 1;
    descSetWrites[ObjBinding::eRenderUniform].pBufferInfo     = &_renderUniformBuffer->descriptor;

    descSetWrites[ObjBinding::eLightMeshIdx].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descSetWrites[ObjBinding::eLightMeshIdx].dstBinding      = ObjBinding::eLightMeshIdx;
    descSetWrites[ObjBinding::eLightMeshIdx].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eLightMeshIdx].descriptorCount = 1;
    descSetWrites[ObjBinding::eLightMeshIdx].pBufferInfo =
        &(_app->_modelLoader.getLightMeshIdxBuffer()->descriptor);

    descSetWrites[ObjBinding::eInstanceBuffer].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descSetWrites[ObjBinding::eInstanceBuffer].dstBinding      = ObjBinding::eInstanceBuffer;
    descSetWrites[ObjBinding::eInstanceBuffer].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eInstanceBuffer].descriptorCount = 1;
    descSetWrites[ObjBinding::eInstanceBuffer].pBufferInfo =
        &(_app->_modelLoader.getInstanceBuffer()->descriptor);

    descSetWrites[ObjBinding::eSceneTexture].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetWrites[ObjBinding::eSceneTexture].dstBinding = ObjBinding::eSceneTexture;
    descSetWrites[ObjBinding::eSceneTexture].dstSet     = _descriptorSet;
    descSetWrites[ObjBinding::eSceneTexture].descriptorCount =
        static_cast<uint32_t>(_app->_modelLoader.getSceneTextures().size());
    std::vector<VkDescriptorImageInfo> imageInfos;
    for (auto& texture : _app->_modelLoader.getSceneTextures())
    {
        imageInfos.push_back(texture->descriptor);
    }
    descSetWrites[ObjBinding::eSceneTexture].pImageInfo = imageInfos.data();

    descSetWrites[ObjBinding::eEnvTexture].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetWrites[ObjBinding::eEnvTexture].dstBinding      = ObjBinding::eEnvTexture;
    descSetWrites[ObjBinding::eEnvTexture].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eEnvTexture].descriptorCount = 1;
    descSetWrites[ObjBinding::eEnvTexture].pImageInfo      = &_envTexture->descriptor;

    descSetWrites[ObjBinding::eEnvLoopupTexture].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetWrites[ObjBinding::eEnvLoopupTexture].dstBinding      = ObjBinding::eEnvLoopupTexture;
    descSetWrites[ObjBinding::eEnvLoopupTexture].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eEnvLoopupTexture].descriptorCount = 1;
    descSetWrites[ObjBinding::eEnvLoopupTexture].pImageInfo      = &_envLookupTexture->descriptor;
    vkUpdateDescriptorSets(_app->m_device, descSetWrites.size(), descSetWrites.data(), 0, nullptr);
}

void RTRenderer::rayTraceRTCreate()
{
    Texture* rayTraceRT;
    auto    textureCreateinfo = nvvk::makeImage2DCreateInfo(
        VkExtent2D{_app->getSize().width, _app->getSize().height}, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        false);
    rayTraceRT             = PlayApp::AllocTexture<Texture>();
    auto samplerCreateInfo = nvvk::makeSamplerCreateInfo(
        VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    auto          cmd         = _app->createTempCmdBuffer();
    nvvk::Texture nvvkTexture = _app->_alloc.createTexture(cmd, 0, nullptr, textureCreateinfo,
                                                     samplerCreateInfo, VK_IMAGE_LAYOUT_GENERAL);
    _app->submitTempCmdBuffer(cmd);
    rayTraceRT->image        = nvvkTexture.image;
    rayTraceRT->memHandle    = nvvkTexture.memHandle;
    rayTraceRT->descriptor   = nvvkTexture.descriptor;
    rayTraceRT->_format      = VK_FORMAT_R32G32B32A32_SFLOAT;
    rayTraceRT->_mipmapLevel = textureCreateinfo.mipLevels;
    _rayTraceRT             = rayTraceRT;
    CUSTOM_NAME_VK(_app->m_debug, _rayTraceRT->image);
}
void RTRenderer::createRenderBuffer()
{
    // Render Uniform Buffer
    _renderUniformBuffer = PlayApp::AllocBuffer<Buffer>();
    VkBufferCreateInfo bufferInfo =
        nvvk::makeBufferCreateInfo(sizeof(RenderUniform), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR);
    auto nvvkBuffer = _app->_alloc.createBuffer(
        bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    _renderUniformBuffer->buffer            = nvvkBuffer.buffer;
    _renderUniformBuffer->address           = nvvkBuffer.address;
    _renderUniformBuffer->memHandle         = nvvkBuffer.memHandle;
    _renderUniformBuffer->descriptor.buffer = nvvkBuffer.buffer;
    _renderUniformBuffer->descriptor.offset = 0;
    _renderUniformBuffer->descriptor.range  = sizeof(RenderUniform);
    CUSTOM_NAME_VK(_app->m_debug, _renderUniformBuffer->buffer);
}
void RTRenderer::createGraphicsPipeline() {}

void RTRenderer::createRTPipeline()
{
    VkPushConstantRange pushConstantRange{}; // empty
    pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                   VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                   VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    pushConstantRange.offset = 0;
    pushConstantRange.size   = 4;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    vkCreatePipelineLayout(_app->m_device, &pipelineLayoutInfo, nullptr, &_rtPipelineLayout);
    CUSTOM_NAME_VK(_app->m_debug, _rtPipelineLayout);
    std::vector<VkPipelineShaderStageCreateInfo> stages;

    VkPipelineShaderStageCreateInfo rayGenStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    rayGenStage.module =
        nvvk::createShaderModule(_app->m_device, nvh::loadFile("spv/raygen.rgen.spv", true));
    rayGenStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    rayGenStage.pName = "main";
    stages.push_back(rayGenStage);
    VkPipelineShaderStageCreateInfo missStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    missStage.module =
        nvvk::createShaderModule(_app->m_device, nvh::loadFile("spv/raymiss.rmiss.spv", true));
    missStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missStage.pName = "main";
    stages.push_back(missStage);

    VkPipelineShaderStageCreateInfo shadowMissStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shadowMissStage.module =
        nvvk::createShaderModule(_app->m_device, nvh::loadFile("spv/shadowmiss.rmiss.spv", true));
    shadowMissStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    shadowMissStage.pName = "main";
    stages.push_back(shadowMissStage);
    VkPipelineShaderStageCreateInfo hitStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    hitStage.module =
        nvvk::createShaderModule(_app->m_device, nvh::loadFile("spv/rayhit.rchit.spv", true));
    hitStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    hitStage.pName = "main";
    stages.push_back(hitStage);

    VkRayTracingShaderGroupCreateInfoKHR rayGroup{
        VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
    rayGroup.closestHitShader   = VK_SHADER_UNUSED_KHR;
    rayGroup.generalShader      = VK_SHADER_UNUSED_KHR;
    rayGroup.anyHitShader       = VK_SHADER_UNUSED_KHR;
    rayGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    rayGroup.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rayGroup.generalShader = 0; // ray gen
    _rtShaderGroups.push_back(rayGroup);
    rayGroup.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    rayGroup.generalShader = 1; // ray miss
    _rtShaderGroups.push_back(rayGroup);
    rayGroup.generalShader = 2; // shadow miss
    _rtShaderGroups.push_back(rayGroup);
    rayGroup.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    rayGroup.generalShader    = VK_SHADER_UNUSED_KHR;
    rayGroup.closestHitShader = 3; // ray hit
    _rtShaderGroups.push_back(rayGroup);

    VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    rtPipelineInfo.maxPipelineRayRecursionDepth = 4;
    rtPipelineInfo.pStages                      = stages.data();
    rtPipelineInfo.stageCount                   = stages.size();
    rtPipelineInfo.groupCount                   = _rtShaderGroups.size();
    rtPipelineInfo.pGroups                      = _rtShaderGroups.data();
    rtPipelineInfo.layout                       = _rtPipelineLayout;

    auto res = vkCreateRayTracingPipelinesKHR(_app->m_device, {}, {}, 1, &rtPipelineInfo, nullptr,
                                              &_rtPipeline);
    assert(res == VK_SUCCESS);
    CUSTOM_NAME_VK(_app->m_debug, _rtPipeline);

    _sbtWrapper.addIndex(nvvk::SBTWrapper::GroupType::eRaygen, 0);
    _sbtWrapper.addIndex(nvvk::SBTWrapper::eMiss, 1);
    _sbtWrapper.addIndex(nvvk::SBTWrapper::eHit, 2);
    _sbtWrapper.create(_rtPipeline, rtPipelineInfo);
}
void RTRenderer::createPostProcessRT(){
    VkImageCreateInfo postProcessRTInfo =  nvvk::makeImage2DCreateInfo(this->_app->getSize(),VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,false);
    VkSamplerCreateInfo postProcessRTSamplerInfo = nvvk::makeSamplerCreateInfo(VK_FILTER_LINEAR,VK_FILTER_LINEAR,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    auto cmd = _app->createTempCmdBuffer();
    _postProcessRT = PlayApp::AllocTexture<Texture>();
    nvvk::Texture nvvkTexture = _app->_alloc.createTexture(cmd,0,nullptr,postProcessRTInfo,postProcessRTSamplerInfo,VK_IMAGE_LAYOUT_UNDEFINED);
    _app->submitTempCmdBuffer(cmd);
    VkImageViewCreateInfo postProcessRTViewInfo = nvvk::makeImage2DViewCreateInfo(nvvkTexture.image,postProcessRTInfo.format,VK_IMAGE_ASPECT_COLOR_BIT,1,nullptr);
    vkCreateImageView(_app->getDevice(),&postProcessRTViewInfo,nullptr,&nvvkTexture.descriptor.imageView);
    _postProcessRT->image        = nvvkTexture.image;
    _postProcessRT->memHandle    = nvvkTexture.memHandle;
    _postProcessRT->descriptor   = nvvkTexture.descriptor;
    _postProcessRT->_format      = postProcessRTInfo.format;
    CUSTOM_NAME_VK(_app->m_debug, _postProcessRT->image);
}

void RTRenderer::createPostProcessRenderPass()
{
    _postProcessRenderPass = nvvk::createRenderPass(
        _app->m_device, {VK_FORMAT_R8G8B8A8_UNORM},VK_FORMAT_UNDEFINED, 1, true, true,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void RTRenderer::createPostProcessFBO()
{
    PlayApp::FreeTexture(_postProcessRT);
    createPostProcessRT();

    std::vector<VkImageView> attachments = {
                                            _postProcessRT->descriptor.imageView};
    VkFramebufferCreateInfo  framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass      = _postProcessRenderPass;
    framebufferInfo.attachmentCount = attachments.size();
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = _app->getSize().width;
    framebufferInfo.height          = _app->getSize().height;
    framebufferInfo.layers          = 1;
    VkResult res =
        vkCreateFramebuffer(_app->m_device, &framebufferInfo, nullptr, &_postProcessFBO);
    assert(res == VK_SUCCESS);
}

void RTRenderer::createPostPipeline()
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &_postDescriptorSetLayout;
    vkCreatePipelineLayout(_app->m_device, &pipelineLayoutInfo, nullptr, &_postPipelineLayout);
    nvvk::GraphicsPipelineGeneratorCombined gpipelineState(_app->m_device, _postPipelineLayout,
                                                           _postProcessRenderPass);
    gpipelineState.inputAssemblyState.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    gpipelineState.rasterizationState.cullMode        = VK_CULL_MODE_NONE;
    gpipelineState.depthStencilState.depthTestEnable  = VK_TRUE;
    gpipelineState.depthStencilState.depthWriteEnable = VK_TRUE;
    VkViewport viewport{0.0f, 0.0f, (float) _app->getSize().width, (float) _app->getSize().height,
                        0.0f, 1.0f};
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE};
    gpipelineState.setDynamicStateEnablesCount(dynamicStates.size());
    for (int i = 0; i < dynamicStates.size(); ++i)
    {
        gpipelineState.setDynamicStateEnable(i, dynamicStates[i]);
    }

    gpipelineState.addShader(nvh::loadFile("spv/post.vert.spv", true), VK_SHADER_STAGE_VERTEX_BIT,
                             "main");
    gpipelineState.addShader(nvh::loadFile("spv/post.frag.spv", true), VK_SHADER_STAGE_FRAGMENT_BIT,
                             "main");

    _postPipeline = gpipelineState.createPipeline();
}

void RTRenderer::createPostDescriptorSet()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo descSetLayoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descSetLayoutInfo.bindingCount = bindings.size();
    descSetLayoutInfo.pBindings    = bindings.data();
    vkCreateDescriptorSetLayout(_app->m_device, &descSetLayoutInfo, nullptr,
                                &_postDescriptorSetLayout);
    CUSTOM_NAME_VK(_app->m_debug, _postDescriptorSetLayout);
    VkDescriptorSetAllocateInfo descSetAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descSetAllocInfo.descriptorPool     = _app->_descriptorPool;
    descSetAllocInfo.descriptorSetCount = 1;
    descSetAllocInfo.pSetLayouts        = &_postDescriptorSetLayout;
    vkAllocateDescriptorSets(_app->m_device, &descSetAllocInfo, &_postDescriptorSet);
    CUSTOM_NAME_VK(_app->m_debug, _postDescriptorSet);
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = _rayTraceRT->descriptor.imageView;
    imageInfo.sampler     = _rayTraceRT->descriptor.sampler;

    VkWriteDescriptorSet descSetWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descSetWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetWrite.dstBinding      = 0;
    descSetWrite.dstSet          = _postDescriptorSet;
    descSetWrite.descriptorCount = 1;
    descSetWrite.pImageInfo      = &imageInfo;
    vkUpdateDescriptorSets(_app->m_device, 1, &descSetWrite, 0, nullptr);
}

void RTRenderer::OnResize(int width, int height)
{
    _frameCount = 0;
    createPostProcessFBO();
    PlayApp::FreeTexture(this->_rayTraceRT);
    rayTraceRTCreate();
    // update raytracing rt
    VkWriteDescriptorSet raytracingRTWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    raytracingRTWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    raytracingRTWrite.dstBinding      = ObjBinding::eRayTraceRT;
    raytracingRTWrite.dstSet          = _descriptorSet;
    raytracingRTWrite.descriptorCount = 1;
    raytracingRTWrite.pImageInfo      = &_rayTraceRT->descriptor;
    vkUpdateDescriptorSets(_app->m_device, 1, &raytracingRTWrite, 0, nullptr);

    // update post descriptor
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = _rayTraceRT->descriptor.imageView;
    imageInfo.sampler     = _rayTraceRT->descriptor.sampler;
    VkWriteDescriptorSet postDescriptorWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    postDescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postDescriptorWrite.dstBinding      = 0;
    postDescriptorWrite.dstSet          = _postDescriptorSet;
    postDescriptorWrite.descriptorCount = 1;
    postDescriptorWrite.pImageInfo      = &imageInfo;
    vkUpdateDescriptorSets(_app->m_device, 1, &postDescriptorWrite, 0, nullptr);

}
void RTRenderer::OnDestroy()
{
    this->_sbtWrapper.destroy();
    this->_shaderModuleManager.deinit();
    this->_rtBuilder.destroy();
    vkDestroyDescriptorSetLayout(_app->m_device, _descriptorSetLayout, nullptr);
    for (auto& blas : this->_blasAccels)
    {
        vkDestroyAccelerationStructureKHR(_app->m_device, blas.accel, nullptr);
        _app->_alloc.destroy(blas.buffer);
    }
    vkDestroyPipeline(_app->m_device, _rtPipeline, nullptr);
}
void RTRenderer::createPostProcessResource(){
    createPostProcessRenderPass();
    createPostProcessFBO();
    createPostDescriptorSet();
    createPostPipeline();
}

} // namespace Play