#include "PlayApp.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui/imgui_helper.h"
#include "nvh/nvprint.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvh/nvprint.hpp"
#include "nvh/fileoperations.hpp"
#include "imgui/imgui_camera_widget.h"
#include "nvp/perproject_globals.hpp"
#include "SceneNode.h"
#include "queue"
#include "iostream"
#include <chrono>
namespace Play
{
struct ScopeTimer
{
    std::chrono::high_resolution_clock::time_point start;
    ScopeTimer() : start(std::chrono::high_resolution_clock::now()) {}
    ~ScopeTimer()
    {
        auto end     = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Scene traversal time: " << elapsed.count() << " ms" << std::endl;
    }
};

void PlayApp::buildTlas()
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    auto                                            meshes = this->_modelLoader.getSceneMeshes();
    std::queue<SceneNode*>                          nodes;
    nodes.push(this->_scene._root.get());
    while (!nodes.empty())
    {
        auto node = nodes.front();
        nodes.pop();

        if (!node->_meshIdx.empty())
        {
            for (int i = 0; i < node->_meshIdx.size(); ++i)
            {
                VkAccelerationStructureInstanceKHR instance{};
                glm::mat4                          Ttransform = glm::transpose(node->_transform);
                memcpy(instance.transform.matrix, &Ttransform[0][0], 12 * sizeof(float));
                instance.mask                = 0xFF;
                instance.instanceCustomIndex = node->_meshIdx[i];
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                instance.instanceShaderBindingTableRecordOffset = 0;
                instance.accelerationStructureReference =
                    this->_blasAccels[node->_meshIdx[i]].address;
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

void PlayApp::buildBlas()
{
    nvvk::BlasBuilder                                 builder(&_alloc, this->m_device);
    auto                                              meshes = this->_modelLoader.getSceneMeshes();
    std::vector<nvvk::AccelerationStructureBuildData> blasBuildDatas;
    std::vector<VkDeviceAddress>                      scratchAddresses;
    uint64_t                                          maxScrashSize = 0;
    for (int b = 0; b < this->_modelLoader.getSceneMeshes().size(); ++b)
    {
        nvvk::AccelerationStructureBuildData blasBuildData;
        VkAccelerationStructureGeometryKHR   geometry{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData.deviceAddress = meshes[b]._vertexAddress;
        geometry.geometry.triangles.vertexStride             = sizeof(Vertex);
        geometry.geometry.triangles.maxVertex                = meshes[b]._vertCnt - 1;
        geometry.geometry.triangles.indexData.deviceAddress  = meshes[b]._indexAddress;
        geometry.geometry.triangles.indexType                = VK_INDEX_TYPE_UINT32;
        // geometry.geometry.triangles.transformData.deviceAddress = {}
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{.primitiveCount  = meshes[b]._faceCnt,
                                                           .primitiveOffset = 0,
                                                           .firstVertex     = 0,
                                                           .transformOffset = 0};
        blasBuildData.addGeometry(geometry, rangeInfo);
        blasBuildData.asType = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        VkAccelerationStructureBuildSizesInfoKHR sizeInfo = blasBuildData.finalizeGeometry(
            this->m_device, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                                VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR |
                                VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        blasBuildDatas.push_back(blasBuildData);
        maxScrashSize = std::max(maxScrashSize, sizeInfo.buildScratchSize);
    };
    scratchAddresses.reserve(blasBuildDatas.size());
    std::vector<nvvk::Buffer> scratchBuffers;
    for (int s = 0; s < blasBuildDatas.size(); ++s)
    {
        scratchBuffers.push_back(_alloc.createBuffer(
            maxScrashSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        scratchAddresses.push_back(scratchBuffers.back().address);
    }
    VkCommandBuffer cmdbuf = createTempCmdBuffer();
    _blasAccels.resize(blasBuildDatas.size());
    bool res =
        builder.cmdCreateParallelBlas(cmdbuf, blasBuildDatas, this->_blasAccels, scratchAddresses);
    submitTempCmdBuffer(cmdbuf);
    assert(res);
    for (auto& scratchBuffer : scratchBuffers)
    {
        _alloc.destroy(scratchBuffer);
    }
    builder.destroy();
}
void PlayApp::createDescritorSet()
{
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    VkDescriptorPoolSize       poolSize[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1024},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
    };
    poolInfo.poolSizeCount = 5;
    poolInfo.pPoolSizes    = poolSize;
    poolInfo.maxSets       = 4096;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    vkCreateDescriptorPool(this->m_device, &poolInfo, nullptr, &_descriptorPool);
    NAME_VK(_descriptorPool);
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
    // // primitive buffer desc binding
    // VkDescriptorSetLayoutBinding primitiveLayoutBinding;
    // primitiveLayoutBinding.binding         = 6;
    // primitiveLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    // primitiveLayoutBinding.descriptorCount = this->_modelLoader.getSceneVBuffers().size();
    // primitiveLayoutBinding.stageFlags      = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
    //                                     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
    //                                     VK_SHADER_STAGE_MISS_BIT_KHR |
    //                                     VK_SHADER_STAGE_FRAGMENT_BIT;

    // scene texture desc binding
    VkDescriptorSetLayoutBinding SceneTextureLayoutBinding;
    SceneTextureLayoutBinding.binding         = ObjBinding::eSceneTexture;
    SceneTextureLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    SceneTextureLayoutBinding.descriptorCount = this->_modelLoader.getSceneTextures().size();
    SceneTextureLayoutBinding.stageFlags =
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT;
    SceneTextureLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        tlasLayoutBinding,           rayTraceRTLayoutBinding,
        materialBufferLayoutBinding, renderUniformBufferLayoutBinding,
        lightMeshIdxLayoutBinding,   instanceBufferLayoutBinding,
        SceneTextureLayoutBinding};
    VkDescriptorSetLayoutCreateInfo descSetLayoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    // descSetLayoutInfo.
    descSetLayoutInfo.bindingCount = bindings.size();
    descSetLayoutInfo.pBindings    = bindings.data();
    descSetLayoutInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    vkCreateDescriptorSetLayout(this->m_device, &descSetLayoutInfo, nullptr, &_descriptorSetLayout);
    NAME_VK(_descriptorSetLayout);
    VkDescriptorSetAllocateInfo descSetAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descSetAllocInfo.descriptorPool     = _descriptorPool;
    descSetAllocInfo.descriptorSetCount = 1;
    descSetAllocInfo.pSetLayouts        = &_descriptorSetLayout;
    vkAllocateDescriptorSets(this->m_device, &descSetAllocInfo, &_descriptorSet);
    NAME_VK(_descriptorSet);

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
    descSetWrites[ObjBinding::eRayTraceRT].pImageInfo      = &_rayTraceRT.descriptor;

    descSetWrites[ObjBinding::eMaterialBuffer].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descSetWrites[ObjBinding::eMaterialBuffer].dstBinding      = ObjBinding::eMaterialBuffer;
    descSetWrites[ObjBinding::eMaterialBuffer].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eMaterialBuffer].descriptorCount = 1;
    descSetWrites[ObjBinding::eMaterialBuffer].pBufferInfo =
        &this->_modelLoader.getMaterialBuffer().descriptor;

    descSetWrites[ObjBinding::eRenderUniform].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descSetWrites[ObjBinding::eRenderUniform].dstBinding      = ObjBinding::eRenderUniform;
    descSetWrites[ObjBinding::eRenderUniform].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eRenderUniform].descriptorCount = 1;
    descSetWrites[ObjBinding::eRenderUniform].pBufferInfo     = &_renderUniformBuffer.descriptor;

    descSetWrites[ObjBinding::eLightMeshIdx].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descSetWrites[ObjBinding::eLightMeshIdx].dstBinding      = ObjBinding::eLightMeshIdx;
    descSetWrites[ObjBinding::eLightMeshIdx].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eLightMeshIdx].descriptorCount = 1;
    descSetWrites[ObjBinding::eLightMeshIdx].pBufferInfo =
        &(this->_modelLoader.getLightMeshIdxBuffer().descriptor);

    descSetWrites[ObjBinding::eInstanceBuffer].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descSetWrites[ObjBinding::eInstanceBuffer].dstBinding      = ObjBinding::eInstanceBuffer;
    descSetWrites[ObjBinding::eInstanceBuffer].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eInstanceBuffer].descriptorCount = 1;
    descSetWrites[ObjBinding::eInstanceBuffer].pBufferInfo =
        &(this->_modelLoader.getInstanceBuffer().descriptor);

    descSetWrites[ObjBinding::eSceneTexture].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetWrites[ObjBinding::eSceneTexture].dstBinding = ObjBinding::eSceneTexture;
    descSetWrites[ObjBinding::eSceneTexture].dstSet     = _descriptorSet;
    descSetWrites[ObjBinding::eSceneTexture].descriptorCount =
        this->_modelLoader.getSceneTextures().size();
    std::vector<VkDescriptorImageInfo> imageInfos;
    for (auto& texture : this->_modelLoader.getSceneTextures())
    {
        imageInfos.push_back(texture.descriptor);
    }
    descSetWrites[ObjBinding::eSceneTexture].pImageInfo = imageInfos.data();
    vkUpdateDescriptorSets(this->m_device, descSetWrites.size(), descSetWrites.data(), 0, nullptr);
}

void PlayApp::createGraphicsPipeline()
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(Constants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    vkCreatePipelineLayout(this->m_device, &pipelineLayoutInfo, nullptr, &_graphicsPipelineLayout);
    NAME_VK(_graphicsPipelineLayout);

    nvvk::GraphicsPipelineGeneratorCombined gpipelineState(this->m_device, _graphicsPipelineLayout,
                                                           this->getRenderPass());
    gpipelineState.inputAssemblyState.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    gpipelineState.rasterizationState.cullMode        = VK_CULL_MODE_NONE;
    gpipelineState.rasterizationState.frontFace       = VK_FRONT_FACE_CLOCKWISE;
    gpipelineState.depthStencilState.depthTestEnable  = VK_TRUE;
    gpipelineState.depthStencilState.depthWriteEnable = VK_TRUE;
    VkViewport viewport{0.0f, 0.0f, (float) this->getSize().width, (float) this->getSize().height,
                        0.0f, 1.0f};
    VkRect2D   scissor{{0, 0}, this->getSize()};
    gpipelineState.viewportState.viewportCount = 1;
    gpipelineState.viewportState.pViewports    = &viewport;
    gpipelineState.viewportState.scissorCount  = 1;
    gpipelineState.viewportState.pScissors     = &scissor;
    std::vector<VkDynamicState> dynamicStates  = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE};
    gpipelineState.setDynamicStateEnablesCount(dynamicStates.size());
    for (int i = 0; i < dynamicStates.size(); ++i)
    {
        gpipelineState.setDynamicStateEnable(i, dynamicStates[i]);
    }
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 24},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, 36},

    };
    gpipelineState.addAttributeDescriptions(vertexInputAttributeDescriptions);
    VkVertexInputBindingDescription vertexInputBindingDescription = {0, sizeof(Vertex),
                                                                     VK_VERTEX_INPUT_RATE_VERTEX};
    gpipelineState.addBindingDescription(vertexInputBindingDescription);
    gpipelineState.addShader(nvh::loadFile("spv/graphic.vert.spv", true),
                             VK_SHADER_STAGE_VERTEX_BIT, "main");
    gpipelineState.addShader(nvh::loadFile("spv/graphic.frag.spv", true),
                             VK_SHADER_STAGE_FRAGMENT_BIT, "main");

    _graphicsPipeline = gpipelineState.createPipeline();
}

void PlayApp::rayTraceRTCreate()
{
    Texture rayTraceRT;
    auto    textureCreateinfo = nvvk::makeImage2DCreateInfo(
        VkExtent2D{this->getSize().width, this->getSize().height}, VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);
    rayTraceRT             = _texturePool.alloc();
    auto samplerCreateInfo = nvvk::makeSamplerCreateInfo(
        VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    auto          cmd         = this->createTempCmdBuffer();
    nvvk::Texture nvvkTexture = _alloc.createTexture(cmd, 0, nullptr, textureCreateinfo,
                                                     samplerCreateInfo, VK_IMAGE_LAYOUT_GENERAL);
    this->submitTempCmdBuffer(cmd);
    rayTraceRT.image        = nvvkTexture.image;
    rayTraceRT.memHandle    = nvvkTexture.memHandle;
    rayTraceRT.descriptor   = nvvkTexture.descriptor;
    rayTraceRT._format      = VK_FORMAT_R32G32B32A32_SFLOAT;
    rayTraceRT._mipmapLevel = textureCreateinfo.mipLevels;
    _rayTraceRT             = rayTraceRT;
    NAME_VK(_rayTraceRT.image);
}

void PlayApp::createRenderBuffer()
{
    // Render Uniform Buffer
    _renderUniformBuffer = _bufferPool.alloc();
    VkBufferCreateInfo bufferInfo =
        nvvk::makeBufferCreateInfo(sizeof(RenderUniform), VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR);
    auto nvvkBuffer = _alloc.createBuffer(
        bufferInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    _renderUniformBuffer.buffer    = nvvkBuffer.buffer;
    _renderUniformBuffer.address   = nvvkBuffer.address;
    _renderUniformBuffer.memHandle = nvvkBuffer.memHandle;
    _renderUniformBuffer.descriptor.buffer = nvvkBuffer.buffer;
    _renderUniformBuffer.descriptor.offset = 0;
    _renderUniformBuffer.descriptor.range  = sizeof(RenderUniform);
    NAME_VK(_renderUniformBuffer.buffer);
}

void PlayApp::OnInit()
{
    _modelLoader.init(this);
    CameraManip.setMode(nvh::CameraManipulator::Modes::Examine);
    CameraManip.setFov(120.0f);
    m_debug.setup(m_device);
    _rtBuilder.setup(m_device, &_alloc, m_graphicsQueueIndex);
    _alloc.init(m_instance, m_device, m_physicalDevice);
    _texturePool.init(2048, &_alloc);
    _bufferPool.init(40960, &_alloc);
    rayTraceRTCreate();
    // _modelLoader.loadModel("F:/repository/ModelResource/gltfBistro/exterior/exterior.gltf");
    _modelLoader.loadModel("F:/repository/ModelResource/gltfBistro/interior/interior.gltf");
    // _modelLoader.loadModel("D:/repo/DogEngine/models/Camera_01_2k/Camera_01_2k.gltf");
    createRenderBuffer();
    buildBlas();
    buildTlas();
    createDescritorSet();
    createGraphicsPipeline();
}
void PlayApp::OnPreRender()
{
    RenderUniform* data = static_cast<RenderUniform*>(_alloc.map(_renderUniformBuffer));
    data->view          = CameraManip.getMatrix();
    data->viewInverse   = glm::inverse(CameraManip.getMatrix());
    data->project       = glm::perspectiveFov(CameraManip.getFov(), this->getSize().width * 1.0f,
                                              this->getSize().height * 1.0f, 0.1f, 10000.0f);
    data->project[1][1] *= -1;
    // {
    //     std::cout << "Projection Matrix:" << std::endl;
    //     for (int i = 0; i < 4; ++i)
    //     {
    //         for (int j = 0; j < 4; ++j) std::cout << data->view[i][j] << ' ';
    //         std::cout << '\n';
    //     }
    //     std::cout << "\r" << std::flush;
    // }
    data->cameraPosition = CameraManip.getEye();
    data->frameCount     = _frameCount++;
    _alloc.unmap(_renderUniformBuffer);
}

void PlayApp::RenderFrame()
{
    auto cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    if (_renderMode == RenderMode::eRayTracing)
    {
        // render ray tracing;
    }
    if (_renderMode == RenderMode::eRasterization)
    {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipelineLayout, 0, 1,
                                &_descriptorSet, 0, nullptr);
        VkClearValue clearValue[2];
        clearValue[0].color        = {{0.0f, 1.0f, 1.0f, 1.0f}};
        clearValue[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassBeginInfo.renderPass = this->getRenderPass();
        renderPassBeginInfo.framebuffer =
            this->getFramebuffers()[this->m_swapChain.getActiveImageIndex()];
        renderPassBeginInfo.renderArea      = VkRect2D({0, 0}, this->getSize());
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues    = clearValue;
        vkCmdBeginRenderPass(cmd, &renderPassBeginInfo,
                             VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
        std::queue<SceneNode*> nodes;
        nodes.push(this->_scene._root.get());
        auto       sceneVBuffers = this->_modelLoader.getSceneVBuffers();
        auto       sceneIBuffers = this->_modelLoader.getSceneIBuffers();
        auto       meshes        = this->_modelLoader.getSceneMeshes();
        VkViewport viewport      = {
            0.0f, 0.0f, (float) this->getSize().width, (float) this->getSize().height, 0.0f, 1.0f};
        VkRect2D scissor = {{0, 0}, this->getSize()};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdSetDepthWriteEnable(cmd, true);
        vkCmdSetDepthTestEnable(cmd, true);
        {
            // ScopeTimer timer;
            while (!nodes.empty())
            {
                SceneNode* currnode = nodes.front();
                nodes.pop();
                if (!currnode->_meshIdx.empty())
                {
                    for (auto& meshIdx : currnode->_meshIdx)
                    {
                        Constants constants;
                        constants.model  = currnode->_transform;
                        constants.matIdx = meshes[meshIdx]._materialIndex;
                        // LOGI(std::to_string(constants.matIdx).c_str());
                        vkCmdPushConstants(
                            cmd, _graphicsPipelineLayout,
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                            sizeof(Constants), &constants);
                        VkDeviceSize offset = 0;
                        vkCmdBindVertexBuffers(cmd, 0, 1,
                                               &(sceneVBuffers[meshes[meshIdx]._vBufferIdx].buffer),
                                               &offset);
                        vkCmdBindIndexBuffer(cmd, sceneIBuffers[meshes[meshIdx]._iBufferIdx].buffer,
                                             0, VkIndexType::VK_INDEX_TYPE_UINT32);
                        vkCmdDrawIndexed(cmd,
                                         this->_modelLoader.getSceneMeshes()[meshIdx]._faceCnt * 3,
                                         1, 0, 0, 0);
                    }
                }
                for (auto& child : currnode->_children)
                {
                    nodes.push(child.get());
                }
            }
        }
        // vkCmdDraw(cmd, 3, 1, 0, 0);
    }
}

void PlayApp::OnPostRender()
{
    auto cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    ImGui::BeginMainMenuBar();
    ImGui::MenuItem("File");
    ImGui::EndMainMenuBar();
    ImGui::ShowDemoWindow();
    ImGui::Render();
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        if (m_window)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(m_window);
        }
    }
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
    ++_frameCount;
}

void PlayApp::onResize(int width, int height)
{
    _texturePool.free(_rayTraceRT);
    rayTraceRTCreate();
    VkWriteDescriptorSet raytracingRTWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    raytracingRTWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    raytracingRTWrite.dstBinding      = ObjBinding::eRayTraceRT;
    raytracingRTWrite.dstSet          = _descriptorSet;
    raytracingRTWrite.descriptorCount = 1;
    raytracingRTWrite.pImageInfo      = &_rayTraceRT.descriptor;
    vkUpdateDescriptorSets(this->m_device, 1, &raytracingRTWrite, 0, nullptr);
}
void PlayApp::Run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        if (this->isMinimized())
        {
            continue;
        }
        this->prepareFrame();
        this->OnPreRender();
        this->RenderFrame();
        this->OnPostRender();
        this->submitFrame();
    }
}

void PlayApp::onDestroy()
{
    this->_texturePool.deinit();
    this->_bufferPool.deinit();
    vkDestroyDescriptorPool(m_device, this->_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, this->_descriptorSetLayout, nullptr);
    _rtBuilder.destroy();
    nvvk::BlasBuilder builder(&_alloc, m_device);
    for (auto& blas : this->_blasAccels)
    {
        vkDestroyAccelerationStructureKHR(m_device, blas.accel, nullptr);
        _alloc.destroy(blas.buffer);
    }
}
} // namespace Play