#include "PlayApp.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "imgui/imgui_helper.h"
#include "imgui/imgui_camera_widget.h"
#include "nvh/nvprint.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/shaders_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "nvh/nvprint.hpp"
#include "nvh/fileoperations.hpp"
#include "nvp/perproject_globals.hpp"
#include "nvh/cameramanipulator.hpp"
#include "nvvkhl/shaders/constants.h"
#include "stb_image.h"
#include "SceneNode.h"
#include "queue"
#include "iostream"
#include "chrono"
#include "numeric"
#include "numbers"
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

void PlayApp::buildBlas()
{
    std::vector<nvvk::RaytracingBuilderKHR::BlasInput> allBlas;
    allBlas.reserve(_modelLoader.getSceneMeshes().size());
    std::vector<Mesh>& meshes = _modelLoader.getSceneMeshes();
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

void PlayApp::createRTPipeline()
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
    vkCreatePipelineLayout(this->m_device, &pipelineLayoutInfo, nullptr, &_rtPipelineLayout);
    NAME_VK(_rtPipelineLayout);
    std::vector<VkPipelineShaderStageCreateInfo> stages;

    VkPipelineShaderStageCreateInfo rayGenStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    rayGenStage.module =
        nvvk::createShaderModule(m_device, nvh::loadFile("spv/raygen.rgen.spv", true));
    rayGenStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    rayGenStage.pName = "main";
    stages.push_back(rayGenStage);
    VkPipelineShaderStageCreateInfo missStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    missStage.module =
        nvvk::createShaderModule(m_device, nvh::loadFile("spv/raymiss.rmiss.spv", true));
    missStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missStage.pName = "main";
    stages.push_back(missStage);

    VkPipelineShaderStageCreateInfo shadowMissStage{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shadowMissStage.module =
        nvvk::createShaderModule(m_device, nvh::loadFile("spv/shadowmiss.rmiss.spv", true));
    shadowMissStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    shadowMissStage.pName = "main";
    stages.push_back(shadowMissStage);
    VkPipelineShaderStageCreateInfo hitStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    hitStage.module =
        nvvk::createShaderModule(m_device, nvh::loadFile("spv/rayhit.rchit.spv", true));
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

    auto res =
        vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rtPipelineInfo, nullptr, &_rtPipeline);
    assert(res == VK_SUCCESS);
    NAME_VK(_rtPipeline);

    _sbtWrapper.addIndex(nvvk::SBTWrapper::GroupType::eRaygen, 0);
    _sbtWrapper.addIndex(nvvk::SBTWrapper::eMiss, 1);
    _sbtWrapper.addIndex(nvvk::SBTWrapper::eHit, 2);
    _sbtWrapper.create(_rtPipeline, rtPipelineInfo);
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

    // scene texture desc binding
    VkDescriptorSetLayoutBinding SceneTextureLayoutBinding;
    SceneTextureLayoutBinding.binding         = ObjBinding::eSceneTexture;
    SceneTextureLayoutBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    SceneTextureLayoutBinding.descriptorCount = this->_modelLoader.getSceneTextures().size();
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

    descSetWrites[ObjBinding::eEnvTexture].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetWrites[ObjBinding::eEnvTexture].dstBinding      = ObjBinding::eEnvTexture;
    descSetWrites[ObjBinding::eEnvTexture].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eEnvTexture].descriptorCount = 1;
    descSetWrites[ObjBinding::eEnvTexture].pImageInfo      = &_envTexture.descriptor;

    descSetWrites[ObjBinding::eEnvLoopupTexture].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetWrites[ObjBinding::eEnvLoopupTexture].dstBinding      = ObjBinding::eEnvLoopupTexture;
    descSetWrites[ObjBinding::eEnvLoopupTexture].dstSet          = _descriptorSet;
    descSetWrites[ObjBinding::eEnvLoopupTexture].descriptorCount = 1;
    descSetWrites[ObjBinding::eEnvLoopupTexture].pImageInfo      = &_envLookupTexture.descriptor;
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
                                                           _rasterizationRenderPass);
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
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        false);
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

void PlayApp::createRazterizationRenderPass()
{
    _rasterizationRenderPass = nvvk::createRenderPass(
        this->m_device, {VK_FORMAT_R32G32B32A32_SFLOAT}, this->m_depthFormat, 1, true, true,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void PlayApp::createRazterizationFBO()
{
    if (_rasterizationDepthImage.image != VK_NULL_HANDLE)
    {
        vkDestroyImage(m_device, _rasterizationDepthImage.image, nullptr);
    }
    if (_rasterizationDepthImage.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_device, _rasterizationDepthImage.memory, nullptr);
    }
    if (_rasterizationDepthImage.view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(m_device, _rasterizationDepthImage.view, nullptr);
    }
    VkImageCreateInfo depthImageCreateInfo = nvvk::makeImage2DCreateInfo(
        VkExtent2D{this->getSize().width, this->getSize().height}, this->m_depthFormat,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);
    vkCreateImage(m_device, &depthImageCreateInfo, nullptr, &_rasterizationDepthImage.image);
    // Allocate the memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, _rasterizationDepthImage.image, &memReqs);
    VkMemoryAllocateInfo memAllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex =
        getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(m_device, &memAllocInfo, nullptr, &_rasterizationDepthImage.memory);
    // Bind image and memory
    vkBindImageMemory(m_device, _rasterizationDepthImage.image, _rasterizationDepthImage.memory, 0);
    auto cmd = this->createTempCmdBuffer();

    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;

    VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imageMemoryBarrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
    imageMemoryBarrier.newLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageMemoryBarrier.image                 = _rasterizationDepthImage.image;
    imageMemoryBarrier.subresourceRange      = subresourceRange;
    imageMemoryBarrier.srcAccessMask         = VkAccessFlags();
    imageMemoryBarrier.dstAccessMask         = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    const VkPipelineStageFlags srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    const VkPipelineStageFlags destStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    vkCmdPipelineBarrier(cmd, srcStageMask, destStageMask, VK_FALSE, 0, nullptr, 0, nullptr, 1,
                         &imageMemoryBarrier);

    this->submitTempCmdBuffer(cmd);
    VkImageViewCreateInfo depthViewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthViewCreateInfo.image            = _rasterizationDepthImage.image;
    depthViewCreateInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    depthViewCreateInfo.format           = this->m_depthFormat;
    depthViewCreateInfo.subresourceRange = subresourceRange;
    _rasterizationDepthImage.layout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vkCreateImageView(m_device, &depthViewCreateInfo, nullptr, &_rasterizationDepthImage.view);

    std::vector<VkImageView> attachments = {_rayTraceRT.descriptor.imageView,
                                            _rasterizationDepthImage.view};
    VkFramebufferCreateInfo  framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass      = _rasterizationRenderPass;
    framebufferInfo.attachmentCount = 2;
    framebufferInfo.pAttachments    = attachments.data();
    framebufferInfo.width           = this->getSize().width;
    framebufferInfo.height          = this->getSize().height;
    framebufferInfo.layers          = 1;
    VkResult res =
        vkCreateFramebuffer(this->m_device, &framebufferInfo, nullptr, &_rasterizationFBO);
    assert(res == VK_SUCCESS);
}

void PlayApp::createPostDescriptorSet()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    VkDescriptorSetLayoutCreateInfo descSetLayoutInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descSetLayoutInfo.bindingCount = bindings.size();
    descSetLayoutInfo.pBindings    = bindings.data();
    vkCreateDescriptorSetLayout(m_device, &descSetLayoutInfo, nullptr, &_postDescriptorSetLayout);
    NAME_VK(_postDescriptorSetLayout);
    VkDescriptorSetAllocateInfo descSetAllocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descSetAllocInfo.descriptorPool     = _descriptorPool;
    descSetAllocInfo.descriptorSetCount = 1;
    descSetAllocInfo.pSetLayouts        = &_postDescriptorSetLayout;
    vkAllocateDescriptorSets(m_device, &descSetAllocInfo, &_postDescriptorSet);
    NAME_VK(_postDescriptorSet);
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = _rayTraceRT.descriptor.imageView;
    imageInfo.sampler     = _rayTraceRT.descriptor.sampler;

    VkWriteDescriptorSet descSetWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descSetWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetWrite.dstBinding      = 0;
    descSetWrite.dstSet          = _postDescriptorSet;
    descSetWrite.descriptorCount = 1;
    descSetWrite.pImageInfo      = &imageInfo;
    vkUpdateDescriptorSets(m_device, 1, &descSetWrite, 0, nullptr);
}

void PlayApp::createPostPipeline()
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &_postDescriptorSetLayout;
    vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &_postPipelineLayout);
    nvvk::GraphicsPipelineGeneratorCombined gpipelineState(this->m_device, _postPipelineLayout,
                                                           m_renderPass);
    gpipelineState.inputAssemblyState.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    gpipelineState.rasterizationState.cullMode        = VK_CULL_MODE_NONE;
    gpipelineState.depthStencilState.depthTestEnable  = VK_TRUE;
    gpipelineState.depthStencilState.depthWriteEnable = VK_TRUE;
    VkViewport viewport{0.0f, 0.0f, (float) this->getSize().width, (float) this->getSize().height,
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

void PlayApp::onKeyboard(int key, int scancode, int action, int mods)
{
    static float silming     = 0.1f;
    const bool   pressed     = action != GLFW_RELEASE;
    auto         cameraPos   = CameraManip.getEye();
    auto         cameraUp    = CameraManip.getUp();
    auto         cameraFront = CameraManip.getCenter();
    // if (pressed && key == GLFW_KEY_W)
    // {
    //     CameraManip.getd
    // }
}

void PlayApp::OnInit()
{
    _modelLoader.init(this);
    CameraManip.setWindowSize(this->getSize().width, this->getSize().height);
    // CameraManip.setMode(nvh::CameraManipulator::Modes::Examine);
    CameraManip.setFov(120.0f);
    CameraManip.setLookat({10.0, 10.0, 10.0}, {0.0, 0.0, 0.0}, {0.000, 1.000, 0.000});
    CameraManip.setSpeed(10.0f);
    // CameraManip
    m_debug.setup(m_device);
    _rtBuilder.setup(m_device, &_alloc, m_graphicsQueueIndex);
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    properties2.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &properties2);
    _shaderModuleManager.init(m_device, 1, 3);

    _sbtWrapper.setup(m_device, this->getQueueFamily(), &_alloc, rtProperties);
    _alloc.init(m_instance, m_device, m_physicalDevice);
    _texturePool.init(2048, &_alloc);
    _bufferPool.init(40960, &_alloc);
    rayTraceRTCreate();

    // _modelLoader.loadModel("F:/repository/ModelResource/gltfBistro/exterior/exterior.gltf");
    // _modelLoader.loadModel("F:/repository/ModelResource/gltfBistro/interior/interior.gltf");
    _modelLoader.loadModel("D:/repo/DogEngine/models/Camera_01_2k/Camera_01_2k.gltf");
    // _modelLoader.loadModel("D:/repo/DogEngine/models/MetalRoughSpheres/MetalRoughSpheres.gltf");
    // _modelLoader.loadModel("D:\\repo\\DogEngine\\models\\DamagedHelmet/DamagedHelmet.gltf");
    loadEnvTexture();
    createRenderBuffer();
    buildBlas();
    buildTlas();
    createDescritorSet();
    createRazterizationRenderPass();
    createRazterizationFBO();
    createGraphicsPipeline();
    createRTPipeline();
    createPostDescriptorSet();
    createPostPipeline();
}
void PlayApp::OnPreRender()
{
    RenderUniform* data = static_cast<RenderUniform*>(_alloc.map(_renderUniformBuffer));
    data->view          = CameraManip.getMatrix();
    data->viewInverse   = glm::inverse(CameraManip.getMatrix());
    data->project       = glm::perspectiveFov(CameraManip.getFov(), this->getSize().width * 1.0f,
                                              this->getSize().height * 1.0f, 0.1f, 10000.0f);
    data->project[1][1] *= -1;
    data->cameraPosition = CameraManip.getEye();
    data->frameCount     = _frameCount++;
    _alloc.unmap(_renderUniformBuffer);
    if (_dirtyCamera != CameraManip.getCamera())
    {
        _frameCount  = -1;
        _dirtyCamera = CameraManip.getCamera();
    }
}

inline float luminance(const float* color)
{
    return color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
}

void PlayApp::loadEnvTexture()
{
    std::string path = "D:\\repo\\DogEngine\\models\\skybox\\graveyard_pathways_2k.hdr";
    // path             = "D:\\repo\\DogEngine\\models\\skybox\\test.hdr";
    int         width, height, channels;
    float*      data = stbi_loadf(path.c_str(), &width, &height, &channels, 4);
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
    _envTexture       = _texturePool.alloc();
    auto          cmd = this->createTempCmdBuffer();
    nvvk::Texture nvvkTexture =
        _alloc.createTexture(cmd, width * height * 4 * sizeof(float), data, imageCreateInfo,
                             samplerCreateInfo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    nvvk::cmdGenerateMipmaps(cmd, nvvkTexture.image, imageCreateInfo.format,
                             {imageCreateInfo.extent.width, imageCreateInfo.extent.height},
                             imageCreateInfo.mipLevels);
    this->submitTempCmdBuffer(cmd);

    _envTexture.image        = nvvkTexture.image;
    _envTexture.memHandle    = nvvkTexture.memHandle;
    _envTexture.descriptor   = nvvkTexture.descriptor;
    _envTexture._format      = imageCreateInfo.format;
    _envTexture._mipmapLevel = 1;

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
    _envLookupTexture          = _texturePool.alloc();
    auto          cmd2         = this->createTempCmdBuffer();
    nvvk::Texture nvvkTexture2 = _alloc.createTexture(
        cmd2, width * height * 4 * sizeof(float), cache.data(), imageCreateInfo2,
        samplerCreateInfo2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    _envLookupTexture.image        = nvvkTexture2.image;
    _envLookupTexture.memHandle    = nvvkTexture2.memHandle;
    _envLookupTexture.descriptor   = nvvkTexture2.descriptor;
    _envLookupTexture._format      = imageCreateInfo2.format;
    _envLookupTexture._mipmapLevel = imageCreateInfo2.mipLevels;
    NAME_VK(_envLookupTexture.image);
    this->submitTempCmdBuffer(cmd2);
    stbi_image_free(data);
}

void PlayApp::RenderFrame()
{
    auto cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    if (_renderMode == RenderMode::eRayTracing)
    {
        VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        imageMemoryBarrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout                   = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.image                       = _rayTraceRT.image;
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.srcAccessMask               = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.srcQueueFamilyIndex = this->getQueueFamily();
        imageMemoryBarrier.dstQueueFamilyIndex = this->getQueueFamily();
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0,
                             nullptr, 1, &imageMemoryBarrier);
        _renderUniformData.frameCount = _frameCount++;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rtPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _rtPipelineLayout, 0,
                                1, &_descriptorSet, 0, nullptr);
        auto regions = _sbtWrapper.getRegions();
        vkCmdTraceRaysKHR(cmd, &regions[0], &regions[1], &regions[2], &regions[3],
                          this->getSize().width, this->getSize().height, 1);

        imageMemoryBarrier.oldLayout                   = VK_IMAGE_LAYOUT_GENERAL;
        imageMemoryBarrier.newLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.image                       = _rayTraceRT.image;
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.subresourceRange.layerCount = 1;
        imageMemoryBarrier.srcAccessMask               = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.srcQueueFamilyIndex = this->getQueueFamily();
        imageMemoryBarrier.dstQueueFamilyIndex = this->getQueueFamily();
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &imageMemoryBarrier);
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
        renderPassBeginInfo.renderPass      = _rasterizationRenderPass;
        renderPassBeginInfo.framebuffer     = _rasterizationFBO;
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
        vkCmdEndRenderPass(cmd);
    }
}

void PlayApp::OnPostRender()
{
    auto cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _postPipeline);
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
    vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetDepthTestEnable(cmd, true);
    vkCmdSetDepthWriteEnable(cmd, true);
    VkViewport viewport = {
        0.0f, 0.0f, (float) this->getSize().width, (float) this->getSize().height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {{0, 0}, this->getSize()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _postPipelineLayout, 0, 1,
                            &_postDescriptorSet, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    ImGui::BeginMainMenuBar();
    ImGui::MenuItem("File");
    ImGui::EndMainMenuBar();

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
    vkDestroyFramebuffer(m_device, _rasterizationFBO, nullptr);
    vkDestroyRenderPass(m_device, _rasterizationRenderPass, nullptr);
    rayTraceRTCreate();
    createRazterizationRenderPass();
    createRazterizationFBO();

    // update raytracing rt
    VkWriteDescriptorSet raytracingRTWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    raytracingRTWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    raytracingRTWrite.dstBinding      = ObjBinding::eRayTraceRT;
    raytracingRTWrite.dstSet          = _descriptorSet;
    raytracingRTWrite.descriptorCount = 1;
    raytracingRTWrite.pImageInfo      = &_rayTraceRT.descriptor;
    vkUpdateDescriptorSets(this->m_device, 1, &raytracingRTWrite, 0, nullptr);

    // update post descriptor
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = _rayTraceRT.descriptor.imageView;
    imageInfo.sampler     = _rayTraceRT.descriptor.sampler;
    VkWriteDescriptorSet postDescriptorWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    postDescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    postDescriptorWrite.dstBinding      = 0;
    postDescriptorWrite.dstSet          = _postDescriptorSet;
    postDescriptorWrite.descriptorCount = 1;
    postDescriptorWrite.pImageInfo      = &imageInfo;
    vkUpdateDescriptorSets(this->m_device, 1, &postDescriptorWrite, 0, nullptr);
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
    this->_sbtWrapper.destroy();
    this->_texturePool.deinit();
    this->_bufferPool.deinit();
    this->_shaderModuleManager.deinit();
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