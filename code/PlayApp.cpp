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
#include "RTRenderer.h"
#include "nvh/container_utils.hpp"
namespace Play
{
nvvk::ResourceAllocatorVma PlayApp::_alloc;
TexturePool                PlayApp::_texturePool;
BufferPool                 PlayApp::_bufferPool;
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

void PlayApp::OnInit()
{
    _modelLoader.init(this);
    CameraManip.setWindowSize(this->getSize().width, this->getSize().height);
    // CameraManip.setMode(nvh::CameraManipulator::Modes::Fly);
    CameraManip.setFov(120.0f);
    CameraManip.setLookat({10.0, 10.0, 10.0}, {0.0, 0.0, 0.0}, {0.000, 1.000, 0.000});
    CameraManip.setSpeed(10.0f);
    // CameraManip
    m_debug.setup(m_device);
    _alloc.init(m_instance, m_device, m_physicalDevice);
    _texturePool.init(65535, &_alloc);
    _bufferPool.init(65535, &_alloc);

    _modelLoader.loadModel("D:\\repo\\DogEngine\\models\\DamagedHelmet/DamagedHelmet.gltf");

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
    if (_renderMode == RenderMode::eRayTracing)
    {
        _renderer = std::make_unique<RTRenderer>(*this);
    }
    createGraphicsPipeline();
}
void PlayApp::OnPreRender()
{
    _renderer->OnPreRender();
}

inline float luminance(const float* color)
{
    return color[0] * 0.2126f + color[1] * 0.7152f + color[2] * 0.0722f;
}

void PlayApp::RenderFrame()
{
    auto cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    _renderer->RenderFrame();
    // if (_renderMode == RenderMode::eRasterization)
    // {
    //     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
    //     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipelineLayout, 0,
    //     1,
    //                             &_descriptorSet, 0, nullptr);
    //     VkClearValue clearValue[2];
    //     clearValue[0].color        = {{0.0f, 1.0f, 1.0f, 1.0f}};
    //     clearValue[1].depthStencil = {1.0f, 0};
    //     VkRenderPassBeginInfo renderPassBeginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    //     renderPassBeginInfo.renderPass      = _rasterizationRenderPass;
    //     renderPassBeginInfo.framebuffer     = _rasterizationFBO;
    //     renderPassBeginInfo.renderArea      = VkRect2D({0, 0}, this->getSize());
    //     renderPassBeginInfo.clearValueCount = 2;
    //     renderPassBeginInfo.pClearValues    = clearValue;
    //     vkCmdBeginRenderPass(cmd, &renderPassBeginInfo,
    //                          VkSubpassContents::VK_SUBPASS_CONTENTS_INLINE);
    //     std::queue<SceneNode*> nodes;
    //     nodes.push(this->_scene._root.get());
    //     auto       sceneVBuffers = this->_modelLoader.getSceneVBuffers();
    //     auto       sceneIBuffers = this->_modelLoader.getSceneIBuffers();
    //     auto       meshes        = this->_modelLoader.getSceneMeshes();
    //     VkViewport viewport      = {
    //         0.0f, 0.0f, (float) this->getSize().width, (float) this->getSize().height,
    //         0.0f, 1.0f};
    //     VkRect2D scissor = {{0, 0}, this->getSize()};
    //     vkCmdSetViewport(cmd, 0, 1, &viewport);
    //     vkCmdSetScissor(cmd, 0, 1, &scissor);
    //     vkCmdSetDepthWriteEnable(cmd, true);
    //     vkCmdSetDepthTestEnable(cmd, true);
    //     {
    //         // ScopeTimer timer;
    //         while (!nodes.empty())
    //         {
    //             SceneNode* currnode = nodes.front();
    //             nodes.pop();
    //             if (!currnode->_meshIdx.empty())
    //             {
    //                 for (auto& meshIdx : currnode->_meshIdx)
    //                 {
    //                     Constants constants;
    //                     constants.model  = currnode->_transform;
    //                     constants.matIdx = meshes[meshIdx]._materialIndex;
    //                     // LOGI(std::to_string(constants.matIdx).c_str());
    //                     vkCmdPushConstants(
    //                         cmd, _graphicsPipelineLayout,
    //                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
    //                         sizeof(Constants), &constants);
    //                     VkDeviceSize offset = 0;
    //                     vkCmdBindVertexBuffers(cmd, 0, 1,
    //                                            &(sceneVBuffers[meshes[meshIdx]._vBufferIdx].buffer),
    //                                            &offset);
    //                     vkCmdBindIndexBuffer(cmd,
    //                     sceneIBuffers[meshes[meshIdx]._iBufferIdx].buffer,
    //                                          0, VkIndexType::VK_INDEX_TYPE_UINT32);
    //                     vkCmdDrawIndexed(cmd,
    //                                      this->_modelLoader.getSceneMeshes()[meshIdx]._faceCnt *
    //                                      3, 1, 0, 0, 0);
    //                 }
    //             }
    //             for (auto& child : currnode->_children)
    //             {
    //                 nodes.push(child.get());
    //             }
    //         }
    //     }
    //     vkCmdEndRenderPass(cmd);
    // }
}

void PlayApp::createRazterizationRenderPass()
{
    // _rasterizationRenderPass = nvvk::createRenderPass(
    //     this->m_device, {VK_FORMAT_R32G32B32A32_SFLOAT}, this->m_depthFormat, 1, true, true,
    //     VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void PlayApp::createRazterizationFBO()
{
    // if (_rasterizationDepthImage.image != VK_NULL_HANDLE)
    // {
    //     vkDestroyImage(m_device, _rasterizationDepthImage.image, nullptr);
    // }
    // if (_rasterizationDepthImage.memory != VK_NULL_HANDLE)
    // {
    //     vkFreeMemory(m_device, _rasterizationDepthImage.memory, nullptr);
    // }
    // if (_rasterizationDepthImage.view != VK_NULL_HANDLE)
    // {
    //     vkDestroyImageView(m_device, _rasterizationDepthImage.view, nullptr);
    // }
    // VkImageCreateInfo depthImageCreateInfo = nvvk::makeImage2DCreateInfo(
    //     VkExtent2D{this->getSize().width, this->getSize().height}, this->m_depthFormat,
    //     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false);
    // vkCreateImage(m_device, &depthImageCreateInfo, nullptr, &_rasterizationDepthImage.image);
    // // Allocate the memory
    // VkMemoryRequirements memReqs;
    // vkGetImageMemoryRequirements(m_device, _rasterizationDepthImage.image, &memReqs);
    // VkMemoryAllocateInfo memAllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    // memAllocInfo.allocationSize = memReqs.size;
    // memAllocInfo.memoryTypeIndex =
    //     getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    // vkAllocateMemory(m_device, &memAllocInfo, nullptr, &_rasterizationDepthImage.memory);
    // // Bind image and memory
    // vkBindImageMemory(m_device, _rasterizationDepthImage.image, _rasterizationDepthImage.memory,
    // 0); auto cmd = this->createTempCmdBuffer();

    // VkImageSubresourceRange subresourceRange{};
    // subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    // subresourceRange.levelCount = 1;
    // subresourceRange.layerCount = 1;

    // VkImageMemoryBarrier imageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    // imageMemoryBarrier.oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
    // imageMemoryBarrier.newLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // imageMemoryBarrier.image                 = _rasterizationDepthImage.image;
    // imageMemoryBarrier.subresourceRange      = subresourceRange;
    // imageMemoryBarrier.srcAccessMask         = VkAccessFlags();
    // imageMemoryBarrier.dstAccessMask         = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    // const VkPipelineStageFlags srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    // const VkPipelineStageFlags destStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // vkCmdPipelineBarrier(cmd, srcStageMask, destStageMask, VK_FALSE, 0, nullptr, 0, nullptr, 1,
    //                      &imageMemoryBarrier);

    // this->submitTempCmdBuffer(cmd);
    // VkImageViewCreateInfo depthViewCreateInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    // depthViewCreateInfo.image            = _rasterizationDepthImage.image;
    // depthViewCreateInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    // depthViewCreateInfo.format           = this->m_depthFormat;
    // depthViewCreateInfo.subresourceRange = subresourceRange;
    // _rasterizationDepthImage.layout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // vkCreateImageView(m_device, &depthViewCreateInfo, nullptr, &_rasterizationDepthImage.view);

    // std::vector<VkImageView> attachments = {_rayTraceRT.descriptor.imageView,
    //                                         _rasterizationDepthImage.view};
    // VkFramebufferCreateInfo  framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    // framebufferInfo.renderPass      = _rasterizationRenderPass;
    // framebufferInfo.attachmentCount = 2;
    // framebufferInfo.pAttachments    = attachments.data();
    // framebufferInfo.width           = this->getSize().width;
    // framebufferInfo.height          = this->getSize().height;
    // framebufferInfo.layers          = 1;
    // VkResult res =
    //     vkCreateFramebuffer(this->m_device, &framebufferInfo, nullptr, &_rasterizationFBO);
    // assert(res == VK_SUCCESS);
}

void PlayApp::createGraphicsPipeline()
{
    // VkPushConstantRange pushConstantRange{};
    // pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    // pushConstantRange.offset     = 0;
    // pushConstantRange.size       = sizeof(Constants);

    // VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    // pipelineLayoutInfo.setLayoutCount = 1;
    // pipelineLayoutInfo.pSetLayouts            = &_descriptorSetLayout;
    // pipelineLayoutInfo.pushConstantRangeCount = 1;
    // pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
    // vkCreatePipelineLayout(this->m_device, &pipelineLayoutInfo, nullptr,
    // &_graphicsPipelineLayout); NAME_VK(_graphicsPipelineLayout);

    // nvvk::GraphicsPipelineGeneratorCombined gpipelineState(this->m_device,
    // _graphicsPipelineLayout,
    //                                                        _rasterizationRenderPass);
    // gpipelineState.inputAssemblyState.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // gpipelineState.rasterizationState.cullMode        = VK_CULL_MODE_NONE;
    // gpipelineState.rasterizationState.frontFace       = VK_FRONT_FACE_CLOCKWISE;
    // gpipelineState.depthStencilState.depthTestEnable  = VK_TRUE;
    // gpipelineState.depthStencilState.depthWriteEnable = VK_TRUE;
    // VkViewport viewport{0.0f, 0.0f, (float) this->getSize().width, (float)
    // this->getSize().height,
    //                     0.0f, 1.0f};
    // VkRect2D   scissor{{0, 0}, this->getSize()};
    // gpipelineState.viewportState.viewportCount = 1;
    // gpipelineState.viewportState.pViewports    = &viewport;
    // gpipelineState.viewportState.scissorCount  = 1;
    // gpipelineState.viewportState.pScissors     = &scissor;
    // std::vector<VkDynamicState> dynamicStates  = {
    //     VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
    //     VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE};
    // gpipelineState.setDynamicStateEnablesCount(dynamicStates.size());
    // for (int i = 0; i < dynamicStates.size(); ++i)
    // {
    //     gpipelineState.setDynamicStateEnable(i, dynamicStates[i]);
    // }
    // std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions = {
    //     {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
    //     {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12},
    //     {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 24},
    //     {3, 0, VK_FORMAT_R32G32_SFLOAT, 36},

    // };
    // gpipelineState.addAttributeDescriptions(vertexInputAttributeDescriptions);
    // VkVertexInputBindingDescription vertexInputBindingDescription = {0, sizeof(Vertex),
    //                                                                  VK_VERTEX_INPUT_RATE_VERTEX};
    // gpipelineState.addBindingDescription(vertexInputBindingDescription);
    // gpipelineState.addShader(nvh::loadFile("spv/graphic.vert.spv", true),
    //                          VK_SHADER_STAGE_VERTEX_BIT, "main");
    // gpipelineState.addShader(nvh::loadFile("spv/graphic.frag.spv", true),
    //                          VK_SHADER_STAGE_FRAGMENT_BIT, "main");

    // _graphicsPipeline = gpipelineState.createPipeline();
}

void PlayApp::OnPostRender()
{
    _renderer->OnPostRender();
    auto         cmd = this->getCommandBuffers()[this->m_swapChain.getActiveImageIndex()];
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
}

void PlayApp::onResize(int width, int height)
{
    vkQueueWaitIdle(this->m_queue);
    _renderer->OnResize(width, height);
    // _texturePool.free(_rayTraceRT);
    // vkDestroyFramebuffer(m_device, _rasterizationFBO, nullptr);
    // vkDestroyRenderPass(m_device, _rasterizationRenderPass, nullptr);
    // rayTraceRTCreate();
    // createRazterizationRenderPass();
    // createRazterizationFBO();

    // // update raytracing rt
    // VkWriteDescriptorSet raytracingRTWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    // raytracingRTWrite.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    // raytracingRTWrite.dstBinding      = ObjBinding::eRayTraceRT;
    // raytracingRTWrite.dstSet          = _descriptorSet;
    // raytracingRTWrite.descriptorCount = 1;
    // raytracingRTWrite.pImageInfo      = &_rayTraceRT.descriptor;
    // vkUpdateDescriptorSets(this->m_device, 1, &raytracingRTWrite, 0, nullptr);

    // // update post descriptor
    // VkDescriptorImageInfo imageInfo;
    // imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // imageInfo.imageView   = _rayTraceRT.descriptor.imageView;
    // imageInfo.sampler     = _rayTraceRT.descriptor.sampler;
    // VkWriteDescriptorSet postDescriptorWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    // postDescriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // postDescriptorWrite.dstBinding      = 0;
    // postDescriptorWrite.dstSet          = _postDescriptorSet;
    // postDescriptorWrite.descriptorCount = 1;
    // postDescriptorWrite.pImageInfo      = &imageInfo;
    // vkUpdateDescriptorSets(this->m_device, 1, &postDescriptorWrite, 0, nullptr);
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
    vkDeviceWaitIdle(m_device);
    _renderer->OnDestroy();
    this->_texturePool.deinit();
    this->_bufferPool.deinit();
    vkDestroyDescriptorPool(m_device, this->_descriptorPool, nullptr);

    _alloc.deinit();
}
} // namespace Play