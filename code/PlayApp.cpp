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
#include "nvvk/shaders_vk.hpp"
#include "nvh/fileoperations.hpp"
#include "nvh/cameramanipulator.hpp"
#include "stb_image.h"
#include "resourceManagement/SceneNode.h"
#include "renderer/RTRenderer.h"
#include "renderer/VolumeRenderer.h"
#include "renderer/ShadingRateRenderer.h"
#include "resourceManagement/Resource.h"
#include "debugger/debugger.h"
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
    // NsightDebugger::initInjection();
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

    _modelLoader.loadModel(".\\resource\\models\\DamagedHelmet/DamagedHelmet.gltf");
    // _modelLoader.loadModel("D:\\repo\\DogEngine\\models\\MetalRoughSpheres\\MetalRoughSpheres.gltf");
    // _modelLoader.loadModel("D:\\repo\\glTF-Sample-Models\\2.0\\ToyCar\\glTF\\ToyCar.gltf");

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
    switch (_renderMode)
    {
        case eRayTracing:
        {
            _renderer = std::make_unique<RTRenderer>(*this);
            break;
        }
        case eVolumeRendering:
        {
            _renderer = std::make_unique<VolumeRenderer>(*this);
            break;
        }
        case eShadingRateRendering:
        {
            _renderer = std::make_unique<ShadingRateRenderer>(*this);
            break;
        }
        default:
        {
            _renderer = std::make_unique<RTRenderer>(*this);
            break;
        }
    }

    createGraphicsDescriptResource();
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
    {
        // if (_renderMode == RenderMode::eRasterization)
        // {
        //     vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
        //     vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        //     _graphicsPipelineLayout, 0, 1,
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
        //                                      this->_modelLoader.getSceneMeshes()[meshIdx]._faceCnt
        //                                      * 3, 1, 0, 0, 0);
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
}

void PlayApp::createGraphicsDescriptResource()
{
    createGraphicsDescriptorSetLayout();
    createGraphicsDescriptorSet();
}
void PlayApp::createGraphicsDescriptorSet()
{
    _graphicDescriptorSet =
        nvvk::allocateDescriptorSet(m_device, _descriptorPool, _graphicsSetLayout);
    VkWriteDescriptorSet writeDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeDescriptorSet.dstSet          = _graphicDescriptorSet;
    writeDescriptorSet.dstBinding      = 0;
    writeDescriptorSet.dstArrayElement = 0;
    auto tempDescriptor                = _renderer->getOutputTexture()->descriptor;
    tempDescriptor.imageLayout         = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    writeDescriptorSet.pImageInfo      = &tempDescriptor;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
}
void PlayApp::createGraphicsDescriptorSetLayout()
{
    nvvk::DescriptorSetBindings bindings;
    bindings.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                        VK_SHADER_STAGE_FRAGMENT_BIT, nullptr);
    _graphicsSetLayout = bindings.createLayout(m_device, 0, nvvk::DescriptorSupport::CORE_1_2);
}
void PlayApp::createGraphicsPipeline()
{
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &_graphicsSetLayout;
    vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &_graphicsPipelineLayout);
    nvvk::GraphicsPipelineGeneratorCombined gpipelineState(m_device, _graphicsPipelineLayout,
                                                           m_renderPass);
    gpipelineState.inputAssemblyState.topology        = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    gpipelineState.rasterizationState.cullMode        = VK_CULL_MODE_NONE;
    gpipelineState.depthStencilState.depthTestEnable  = VK_TRUE;
    gpipelineState.depthStencilState.depthWriteEnable = VK_TRUE;
    VkViewport viewport{0.0f, 0.0f, (float) getSize().width, (float) getSize().height, 0.0f, 1.0f};
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};
    gpipelineState.setDynamicStateEnablesCount(dynamicStates.size());
    for (int i = 0; i < dynamicStates.size(); ++i)
    {
        gpipelineState.setDynamicStateEnable(i, dynamicStates[i]);
    }

    gpipelineState.addShader(nvh::loadFile("spv/post.vert.spv", true), VK_SHADER_STAGE_VERTEX_BIT,
                             "main");
    gpipelineState.addShader(nvh::loadFile("spv/outputBlit.frag.spv", true),
                             VK_SHADER_STAGE_FRAGMENT_BIT, "main");

    _graphicsPipeline = gpipelineState.createPipeline();
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
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
    VkViewport viewport{0.0f, 0.0f, (float) getSize().width, (float) getSize().height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {{0, 0}, this->getSize()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipelineLayout, 0, 1,
                            &_graphicDescriptorSet, 0, nullptr);
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
}
Texture* PlayApp::CreateTexture(VkImageCreateInfo& info, VkCommandBuffer* cmd)
{
    if (!cmd) *cmd = createTempCmdBuffer();
    Texture*      texture     = _texturePool.alloc<Texture>();
    auto          samplerInfo = nvvk::makeSamplerCreateInfo();
    nvvk::Texture nvvkTexture =
        _alloc.createTexture(*cmd, 0, nullptr, info, samplerInfo, VK_IMAGE_LAYOUT_UNDEFINED);
    texture->image      = nvvkTexture.image;
    texture->memHandle  = nvvkTexture.memHandle;
    texture->descriptor = nvvkTexture.descriptor;
    texture->_format    = info.format;
    NAME_VK(texture->image);
    submitTempCmdBuffer(*cmd);
    return texture;
}
Buffer* PlayApp::CreateBuffer(VkBufferCreateInfo& info, VkMemoryPropertyFlags memProperties)
{
    Buffer*      buffer     = _bufferPool.alloc<Buffer>();
    nvvk::Buffer nvvkBuffer = _alloc.createBuffer(info, memProperties);
    buffer->buffer          = nvvkBuffer.buffer;
    buffer->address         = nvvkBuffer.address;
    buffer->memHandle       = nvvkBuffer.memHandle;
    NAME_VK(buffer->buffer);
    return buffer;
}
void PlayApp::onResize(int width, int height)
{
    vkQueueWaitIdle(this->m_queue);
    _renderer->OnResize(width, height);

    // update graphics descriptor set
    VkDescriptorImageInfo imageInfo;
    auto                  tempDescriptor = _renderer->getOutputTexture()->descriptor;
    imageInfo.imageLayout                = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView                  = tempDescriptor.imageView;
    imageInfo.sampler                    = tempDescriptor.sampler;
    VkWriteDescriptorSet writeDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeDescriptorSet.dstSet          = _graphicDescriptorSet;
    writeDescriptorSet.dstBinding      = 0;
    writeDescriptorSet.dstArrayElement = 0;
    writeDescriptorSet.pImageInfo      = &imageInfo;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
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